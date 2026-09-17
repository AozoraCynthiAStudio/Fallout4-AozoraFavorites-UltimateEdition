#include "Aozora/FavoriteStore.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <regex>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "F4SE/F4SE.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/B/BGSInventoryInterface.h"
#include "RE/B/BGSInventoryList.h"
#include "RE/B/BGSKeywordForm.h"
#include "RE/B/BGSObjectInstanceExtra.h"
#include "RE/E/ExtraFavorite.h"
#include "RE/F/FavoritesManager.h"
#include "RE/Fallout.h"
#include "RE/I/InventoryInterface.h"
#include "RE/P/PipboyMenu.h"
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"
#include "RE/U/UI_MESSAGE_TYPE.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "Aozora/ScaleformMenu.h"
#include "Aozora/Settings.h"

namespace Aozora::SWF
{
    namespace
    {
        constexpr std::uint32_t QUICKKEY_NONE = 0xFFFFFFFFu;
        constexpr std::uint32_t SERIALIZATION_ID = 'AoFM';
        constexpr std::uint32_t USAGE_RECORD = 'USAG';
        constexpr std::uint32_t USAGE_RECORD_VERSION = 1;
        constexpr std::uint32_t FAVORITES_V2_RECORD = 'FAV2';
        constexpr std::uint32_t FAVORITES_V2_RECORD_VERSION = 2;

        struct Identity
        {
            std::uint32_t formID{ 0 };
            std::uint64_t instanceKey{ 0 };

            friend bool operator==(const Identity& a_lhs, const Identity& a_rhs) noexcept
            {
                return a_lhs.formID == a_rhs.formID && a_lhs.instanceKey == a_rhs.instanceKey;
            }
        };

        struct IdentityHash
        {
            std::size_t operator()(const Identity& a_identity) const noexcept
            {
                auto value = a_identity.instanceKey ^
                    (static_cast<std::uint64_t>(a_identity.formID) + 0x9E3779B97F4A7C15ull +
                        (a_identity.instanceKey << 6) + (a_identity.instanceKey >> 2));
                value ^= value >> 30;
                value *= 0xBF58476D1CE4E5B9ull;
                value ^= value >> 27;
                value *= 0x94D049BB133111EBull;
                value ^= value >> 31;
                return static_cast<std::size_t>(value);
            }
        };

        struct StoredFavorite
        {
            std::uint32_t formID{ 0 };
            std::uint64_t instanceKey{ 0 };
            std::uint32_t hotkeySlot{ QUICKKEY_NONE };
            std::uint32_t addOrder{ 0 };
        };

        struct FISIconRule
        {
            std::string keyword;
            std::string iconLibrary;
            std::string iconClass;
        };

        struct AutoTagRule
        {
            std::string section;
            std::string pattern;
            std::string tag;
            bool wildcard{ false };
            std::optional<std::regex> expression;
        };

        void Log(std::string_view a_message);

        std::vector<FISIconRule> g_fisIconRules;
        bool g_fisIconRulesLoaded{ false };
        std::vector<AutoTagRule> g_autoTagRules;
        bool g_autoTagRulesLoaded{ false };
        std::size_t g_autoTagFileCount{ 0 };
        std::unordered_map<std::uint32_t, std::string> g_lastLoggedFisIcons;
        std::unordered_map<std::uint32_t, std::string> g_lastLoggedEquipMatches;

        std::string TrimAscii(std::string value)
        {
            const auto isSpace = [](unsigned char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
                value.erase(value.begin());
            }
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
                value.pop_back();
            }
            return value;
        }

        std::string LowerAscii(std::string value)
        {
            for (auto& ch : value) {
                if (ch >= 'A' && ch <= 'Z') {
                    ch = static_cast<char>(ch - 'A' + 'a');
                }
            }
            return value;
        }

        std::optional<std::string> XmlAttribute(std::string_view element, std::string_view name)
        {
            std::size_t search = 0;
            while ((search = element.find(name, search)) != std::string_view::npos) {
                const auto before = search == 0 ? '\0' : element[search - 1];
                const auto after = search + name.size() < element.size() ? element[search + name.size()] : '\0';
                const auto identifier = [](char ch) {
                    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
                };
                if (identifier(before) || identifier(after)) {
                    search += name.size();
                    continue;
                }
                auto cursor = search + name.size();
                while (cursor < element.size() &&
                    (element[cursor] == ' ' || element[cursor] == '\t' || element[cursor] == '\r' || element[cursor] == '\n')) {
                    ++cursor;
                }
                if (cursor >= element.size() || element[cursor] != '=') {
                    search += name.size();
                    continue;
                }
                ++cursor;
                while (cursor < element.size() &&
                    (element[cursor] == ' ' || element[cursor] == '\t' || element[cursor] == '\r' || element[cursor] == '\n')) {
                    ++cursor;
                }
                if (cursor >= element.size() || (element[cursor] != '\'' && element[cursor] != '"')) {
                    return std::nullopt;
                }
                const auto quote = element[cursor++];
                const auto end = element.find(quote, cursor);
                if (end == std::string_view::npos) {
                    return std::nullopt;
                }
                return std::string(element.substr(cursor, end - cursor));
            }
            return std::nullopt;
        }

        bool IsXmlTagStart(std::string_view text, std::size_t position, std::string_view tagName)
        {
            if (position == std::string_view::npos || text.compare(position, tagName.size(), tagName) != 0) {
                return false;
            }
            const auto after = position + tagName.size();
            return after >= text.size() || text[after] == ' ' || text[after] == '\t' ||
                text[after] == '\r' || text[after] == '\n' || text[after] == '>' || text[after] == '/';
        }

        std::size_t ParseFISXml(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return 0;
            }
            const std::string xml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            std::size_t ruleCount = 0;
            std::size_t cursor = 0;
            while ((cursor = xml.find("<tags", cursor)) != std::string::npos) {
                if (!IsXmlTagStart(xml, cursor, "<tags")) {
                    cursor += 5;
                    continue;
                }
                const auto openEnd = xml.find('>', cursor);
                if (openEnd == std::string::npos) {
                    break;
                }
                const auto close = xml.find("</tags>", openEnd + 1);
                if (close == std::string::npos) {
                    break;
                }
                const auto library = XmlAttribute(std::string_view(xml).substr(cursor, openEnd - cursor + 1), "iconLibraryFile");
                // The FIS file contains a documentation comment with a
                // literal <tags> example before the real tag block. If this
                // block has no icon library, keep scanning instead of jumping
                // over the real block at the comment's closing </tags>.
                if (!library || library->empty()) {
                    cursor = openEnd + 1;
                    continue;
                }
                std::size_t tagCursor = openEnd + 1;
                while ((tagCursor = xml.find("<tag", tagCursor)) != std::string::npos && tagCursor < close) {
                    if (!IsXmlTagStart(xml, tagCursor, "<tag")) {
                        tagCursor += 4;
                        continue;
                    }
                    const auto tagEnd = xml.find('>', tagCursor);
                    if (tagEnd == std::string::npos || tagEnd >= close) {
                        break;
                    }
                    const auto element = std::string_view(xml).substr(tagCursor, tagEnd - tagCursor + 1);
                    const auto keyword = XmlAttribute(element, "keyword");
                    const auto icon = XmlAttribute(element, "icon");
                    if (keyword && icon && !keyword->empty() && !icon->empty()) {
                        g_fisIconRules.push_back({ TrimAscii(*keyword), TrimAscii(*library), TrimAscii(*icon) });
                        ++ruleCount;
                    }
                    tagCursor = tagEnd + 1;
                }
                cursor = close + 7;
            }
            return ruleCount;
        }

        void LoadFISIconRules()
        {
            if (g_fisIconRulesLoaded) {
                return;
            }
            g_fisIconRulesLoaded = true;
            g_fisIconRules.clear();

            const std::vector<std::filesystem::path> baseCandidates{
                "Data/Interface/ItemSorter/FIS (FallUI Item Sorter).xml",
                "Data/Interface/ItemSorter/FIS/FIS (FallUI Item Sorter).xml",
                "Interface/ItemSorter/FIS (FallUI Item Sorter).xml",
                "Interface/ItemSorter/FIS/FIS (FallUI Item Sorter).xml"
            };
            std::filesystem::path basePath;
            std::size_t baseRules = 0;
            std::error_code baseError;
            for (const auto& candidate : baseCandidates) {
                if (std::filesystem::is_regular_file(candidate, baseError)) {
                    basePath = candidate;
                    baseRules = ParseFISXml(basePath);
                    break;
                }
            }
            std::size_t addonRules = 0;
            std::size_t addonFiles = 0;
            const std::filesystem::path addonPath{ "Data/Interface/ItemSorter/FIS/TagConfiguration.Addons" };
            std::error_code error;
            if (std::filesystem::exists(addonPath, error) && std::filesystem::is_directory(addonPath, error)) {
                std::vector<std::filesystem::path> files;
                for (const auto& entry : std::filesystem::directory_iterator(addonPath, error)) {
                    if (error || !entry.is_regular_file(error)) {
                        continue;
                    }
                    const auto extension = LowerAscii(entry.path().extension().string());
                    if (extension == ".xml") {
                        files.push_back(entry.path());
                    }
                }
                std::sort(files.begin(), files.end());
                for (const auto& file : files) {
                    addonRules += ParseFISXml(file);
                    ++addonFiles;
                }
            }
            Log("FIS_CONFIG loaded=" + std::to_string(baseRules > 0 ? 1 : 0) +
                " basePath=" + (basePath.empty() ? "none" : basePath.string()) +
                " baseRules=" + std::to_string(baseRules) +
                " addonFiles=" + std::to_string(addonFiles) +
                " addonRules=" + std::to_string(addonRules) +
                " totalRules=" + std::to_string(g_fisIconRules.size()));
        }

        std::size_t ParseAutoTagsIni(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return 0;
            }
            std::string section;
            std::size_t ruleCount = 0;
            std::string line;
            while (std::getline(file, line)) {
                if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                    static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
                    line.erase(0, 3);
                }
                line = TrimAscii(std::move(line));
                if (line.empty() || line.front() == ';' || line.front() == '#') {
                    continue;
                }
                if (line.front() == '[' && line.back() == ']') {
                    section = LowerAscii(TrimAscii(line.substr(1, line.size() - 2)));
                    continue;
                }
                const auto separator = line.find('=');
                if (section.empty() || separator == std::string::npos) {
                    continue;
                }
                const auto pattern = TrimAscii(line.substr(0, separator));
                const auto value = TrimAscii(line.substr(separator + 1));
                const auto tagStart = value.find('[');
                const auto tagEnd = tagStart == std::string::npos ? std::string::npos : value.find(']', tagStart + 1);
                if (pattern.empty() || tagStart == std::string::npos || tagEnd <= tagStart + 1) {
                    continue;
                }
                AutoTagRule rule;
                rule.section = section;
                rule.pattern = pattern;
                rule.tag = TrimAscii(value.substr(tagStart + 1, tagEnd - tagStart - 1));
                rule.wildcard = pattern == "*";
                if (!rule.wildcard) {
                    try {
                        rule.expression.emplace(rule.pattern, std::regex::icase);
                    } catch (const std::regex_error&) {
                        continue;
                    }
                }
                g_autoTagRules.push_back(std::move(rule));
                ++ruleCount;
            }
            return ruleCount;
        }

        void LoadAutoTagRules()
        {
            if (g_autoTagRulesLoaded) {
                return;
            }
            g_autoTagRulesLoaded = true;
            g_autoTagRules.clear();
            const std::vector<std::filesystem::path> candidates{
                "Data/Interface/ItemSorter/FIS/AutoTags/AutoTags_en.ini",
                "Data/Interface/ItemSorter/FIS/AutoTags/AutoTags_cn.ini",
                "Interface/ItemSorter/FIS/AutoTags/AutoTags_en.ini",
                "Interface/ItemSorter/FIS/AutoTags/AutoTags_cn.ini"
            };
            std::size_t totalRules = 0;
            std::error_code error;
            for (const auto& candidate : candidates) {
                if (std::filesystem::is_regular_file(candidate, error)) {
                    totalRules += ParseAutoTagsIni(candidate);
                    ++g_autoTagFileCount;
                }
            }
            Log("FIS_AUTOTAGS files=" + std::to_string(g_autoTagFileCount) +
                " rules=" + std::to_string(totalRules));
        }

        std::pair<std::string, std::string> FISIconForTag(std::string_view tag)
        {
            LoadFISIconRules();
            for (auto rule = g_fisIconRules.rbegin(); rule != g_fisIconRules.rend(); ++rule) {
                if (rule->keyword == tag) {
                    return { rule->iconLibrary, rule->iconClass };
                }
            }
            return {};
        }

        std::string AutoTagSectionForObject(RE::TESBoundObject* object)
        {
            if (!object) {
                return {};
            }
            switch (object->GetFormType()) {
            case RE::ENUM_FORM_ID::kWEAP: return "weap";
            case RE::ENUM_FORM_ID::kARMO: return "armo";
            case RE::ENUM_FORM_ID::kALCH: return "alch";
            case RE::ENUM_FORM_ID::kAMMO: return "ammo";
            case RE::ENUM_FORM_ID::kMISC: return "misc";
            case RE::ENUM_FORM_ID::kNOTE: return "note";
            case RE::ENUM_FORM_ID::kBOOK: return "book";
            case RE::ENUM_FORM_ID::kOMOD: return "omod";
            case RE::ENUM_FORM_ID::kCMPO: return "cmpo";
            case RE::ENUM_FORM_ID::kINGR: return "ingr";
            case RE::ENUM_FORM_ID::kKEYM: return "keym";
            default: return {};
            }
        }

        std::pair<std::string, std::string> FISIconForAutoTag(std::string_view name,
            std::string_view section)
        {
            LoadAutoTagRules();
            const auto matches = [&](const AutoTagRule& rule) {
                if (rule.section != section || rule.wildcard || !rule.expression) {
                    return false;
                }
                return std::regex_search(name.begin(), name.end(), *rule.expression);
            };
            for (const auto& rule : g_autoTagRules) {
                if (matches(rule)) {
                    const auto icon = FISIconForTag(rule.tag);
                    if (!icon.second.empty()) {
                        return icon;
                    }
                }
            }
            for (const auto& rule : g_autoTagRules) {
                if (rule.section == section && rule.wildcard) {
                    const auto icon = FISIconForTag(rule.tag);
                    if (!icon.second.empty()) {
                        return icon;
                    }
                }
            }
            return {};
        }

        std::pair<std::string, std::string> FISIconForObject(RE::TESBoundObject* object)
        {
            LoadFISIconRules();
            if (!object) {
                return {};
            }
            const auto* keywordForm = object->As<RE::BGSKeywordForm>();
            if (!keywordForm) {
                return {};
            }
            // Later and more specific FIS rules override the base rule.
            for (auto rule = g_fisIconRules.rbegin(); rule != g_fisIconRules.rend(); ++rule) {
                if (keywordForm->HasKeywordString(rule->keyword)) {
                    return { rule->iconLibrary, rule->iconClass };
                }
            }
            return {};
        }

        std::pair<std::string, std::string> FISIconForName(std::string_view name)
        {
            LoadFISIconRules();
            std::size_t start = 0;
            while (start < name.size() &&
                (name[start] == ' ' || name[start] == '\t' || name[start] == '\r' || name[start] == '\n')) {
                ++start;
            }
            if (start >= name.size() || std::string_view("|{[(").find(name[start]) == std::string_view::npos) {
                return {};
            }
            const auto end = name.find_first_of("|}])", start + 1);
            if (end == std::string_view::npos || end <= start + 1) {
                return {};
            }
            const auto tag = TrimAscii(std::string(name.substr(start + 1, end - start - 1)));
            for (auto rule = g_fisIconRules.rbegin(); rule != g_fisIconRules.rend(); ++rule) {
                if (rule->keyword == tag) {
                    return { rule->iconLibrary, rule->iconClass };
                }
            }
            return {};
        }

        struct InventoryRef
        {
            RE::BGSInventoryItem* item{ nullptr };
            RE::BGSInventoryItem::Stack* stack{ nullptr };
            std::uint32_t stackID{ 0 };
        };

        std::mutex g_mutex;
        std::vector<StoredFavorite> g_favorites;
        std::unordered_map<Identity, std::size_t, IdentityHash> g_index;
        std::unordered_map<std::uint32_t, std::uint32_t> g_useCounts;
        std::uint32_t g_nextAddOrder{ 1 };
        bool g_initialized{ false };
        bool g_legacyMarkerMigrationAttempted{ false };
        std::atomic_bool g_internalFavoriteUpdate{ false };
        std::atomic<std::uint64_t> g_internalFavoriteUpdateUntil{ 0 };
        std::uint32_t g_weaponSwitchSerial{ 0 };
        StoredFavorite g_weaponSwitchFavorite{};
        bool g_weaponSwitchActive{ false };
        bool g_weaponSwitchEquipIssued{ false };
        bool g_weaponSwitchDrawRequested{ false };
        bool g_weaponSwitchUnequip{ false };
        bool g_weaponSwitchUnequipIssued{ false };
        bool g_weaponSwitchRawResult{ false };

        void Log(std::string_view a_message)
        {
            REX::INFO("[AozoraFavoritesSWF] {}", a_message);
            std::ofstream file("Data/F4SE/Plugins/AozoraFavoritesSWF.log", std::ios::app);
            if (file.is_open()) {
                file << '[' << GetTickCount64() << "] STORE " << a_message << '\n';
            }
        }

        void ExtendInternalFavoriteUpdate(std::uint64_t a_durationMs = 1000)
        {
            const auto deadline = GetTickCount64() + a_durationMs;
            auto current = g_internalFavoriteUpdateUntil.load();
            while (current < deadline &&
                !g_internalFavoriteUpdateUntil.compare_exchange_weak(current, deadline)) {}
        }

        std::uint64_t HashValue(std::uint64_t a_hash, std::uint64_t a_value)
        {
            a_hash ^= a_value + 0x9E3779B97F4A7C15ull + (a_hash << 6) + (a_hash >> 2);
            a_hash *= 0x100000001B3ull;
            return a_hash;
        }

        std::uint64_t InstanceKeyForStack(const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID)
        {
            if (!a_item.object ||
                (!a_item.object->Is(RE::ENUM_FORM_ID::kWEAP) &&
                    !a_item.object->Is(RE::ENUM_FORM_ID::kARMO))) {
                return 0;
            }

            constexpr std::uint64_t FNV_OFFSET = 1469598103934665603ull;
            auto hash = HashValue(FNV_OFFSET, a_item.object->GetFormID());
            const auto* stack = a_item.GetStackByID(a_stackID);
            if (!stack) {
                return hash;
            }
            if (auto* objectExtra = stack->extra ?
                    stack->extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
                objectExtra && objectExtra->values) {
                for (const auto& data : objectExtra->GetIndexData()) {
                    hash = HashValue(hash, data.objectID);
                    hash = HashValue(hash, data.index);
                    hash = HashValue(hash, data.rank);
                    hash = HashValue(hash, data.disabled ? 1 : 0);
                }
            }
            if (stack->extra) {
                if (auto* legendary = stack->extra->GetLegendaryMod()) {
                    hash = HashValue(hash, 0x4C4547454E444152ull);
                    hash = HashValue(hash, legendary->GetFormID());
                }
            }
            const char* display = stack && stack->extra ?
                a_item.GetDisplayFullName(stack->extra.get()) :
                a_item.GetDisplayFullName(a_stackID);
            if (display && display[0]) {
                for (const auto ch : std::string_view(display)) {
                    hash = HashValue(hash, static_cast<unsigned char>(ch));
                }
                hash = HashValue(hash, 0);
            }
            return hash == 0 ? 1 : hash;
        }

        InventoryRef FindInventoryStack(RE::TESBoundObject* a_object, std::uint64_t a_instanceKey)
        {
            InventoryRef fallback;
            InventoryRef soleStack;
            std::uint32_t stackCount = 0;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList || !a_object) {
                return fallback;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_object->GetFormID()) {
                    continue;
                }
                std::uint32_t stackID = 0;
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                    soleStack = { std::addressof(item), stack, stackID };
                    ++stackCount;
                    if (!fallback.item) {
                        fallback = { std::addressof(item), stack, stackID };
                    }
                    if (a_instanceKey != 0 && InstanceKeyForStack(item, stackID) == a_instanceKey) {
                        return { std::addressof(item), stack, stackID };
                    }
                    if (a_instanceKey == 0 && stack->IsEquipped()) {
                        fallback = { std::addressof(item), stack, stackID };
                    }
                }
            }
            // A stale display-name component can change an instance hash
            // after a rename. Falling back is safe only when this form has a
            // single inventory stack; with split stacks we must refuse to
            // guess and risk equipping the wrong weapon.
            if (a_instanceKey != 0) {
                return stackCount == 1 ? soleStack : InventoryRef{};
            }
            return fallback;
        }

        std::uint32_t InventoryCountForForm(RE::TESBoundObject* a_object)
        {
            std::uint32_t count = 0;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList || !a_object) {
                return 0;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_object->GetFormID()) {
                    continue;
                }
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
                    count += stack->GetCount();
                }
            }
            return count;
        }

        bool IsEquippedForIdentity(RE::TESBoundObject* a_object, std::uint64_t a_instanceKey,
            bool a_uniqueFavoriteForForm)
        {
            if (!a_object) {
                return false;
            }
            if (a_instanceKey == 0) {
                const auto selected = FindInventoryStack(a_object, 0);
                return selected.stack && selected.stack->IsEquipped();
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return false;
            }
            bool matchedIdentity = false;
            bool matchedEquipped = false;
            std::uint32_t equippedStackCount = 0;
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_object->GetFormID()) {
                    continue;
                }
                std::uint32_t stackID = 0;
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                    if (stack->IsEquipped()) {
                        ++equippedStackCount;
                    }
                    if (InstanceKeyForStack(item, stackID) == a_instanceKey) {
                        matchedIdentity = true;
                        matchedEquipped = matchedEquipped || stack->IsEquipped();
                    }
                }
            }
            if (matchedIdentity) {
                return matchedEquipped;
            }
            // A stale key can survive a save/load or a display-name refresh.
            // Resolve it only when the form has one favorite and exactly one
            // equipped stack; never mark every same-form favorite by default.
            return a_uniqueFavoriteForForm && equippedStackCount == 1;
        }

        InventoryRef FindEquippedInstance(
            RE::TESBoundObject* a_object,
            std::uint64_t a_excludedInstanceKey)
        {
            InventoryRef result;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList || !a_object) {
                return result;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_object->GetFormID()) {
                    continue;
                }
                std::uint32_t stackID = 0;
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                    if (stack->IsEquipped() && InstanceKeyForStack(item, stackID) != a_excludedInstanceKey) {
                        return { std::addressof(item), stack, stackID };
                    }
                }
            }
            return result;
        }

        bool IsEquipped(RE::TESBoundObject* a_object, std::uint64_t a_instanceKey)
        {
            const auto selected = FindInventoryStack(a_object, a_instanceKey);
            return selected.stack && selected.stack->IsEquipped();
        }

        bool IsTargetEquipped(std::uint32_t a_formID, std::uint64_t a_instanceKey)
        {
            auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID);
            if (!object) {
                return false;
            }
            const auto favoriteCount = std::count_if(
                g_favorites.begin(), g_favorites.end(),
                [a_formID](const StoredFavorite& a_entry) { return a_entry.formID == a_formID; });
            // This is the same identity-aware test used by Snapshot() for
            // EQUIP_MATCH, including the unique-favorite stale-key fallback.
            return IsEquippedForIdentity(object, a_instanceKey, favoriteCount == 1);
        }

        bool EquipWeaponDirectOnce(RE::TESBoundObject* a_object, std::uint64_t a_instanceKey)
        {
            if (!a_object || !a_object->Is(RE::ENUM_FORM_ID::kWEAP)) {
                return false;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* manager = RE::ActorEquipManager::GetSingleton();
            const auto selected = FindInventoryStack(a_object, a_instanceKey);
            if (!player || !manager || !selected.item || !selected.stack) {
                Log("weapon equip skipped: inventory stack missing");
                return false;
            }
            RE::BGSObjectInstance instance(a_object, selected.item->GetInstanceData(selected.stackID));
            const bool result = manager->EquipObject(player, instance, selected.stackID, 1, nullptr,
                false, false, true, true, false);
            Log("weapon equip request form=" + std::to_string(a_object->GetFormID()) +
                " stack=" + std::to_string(selected.stackID) +
                " result=" + std::to_string(result ? 1 : 0));
            return result;
        }

        void FinishWeaponSwitch(std::uint32_t a_serial, [[maybe_unused]] bool a_countUse)
        {
            if (!g_weaponSwitchActive || a_serial != g_weaponSwitchSerial) {
                return;
            }
            const auto formID = g_weaponSwitchFavorite.formID;
            const auto instanceKey = g_weaponSwitchFavorite.instanceKey;
            const bool shouldBeEquipped = !g_weaponSwitchUnequip;
            const bool afterEquipped = IsTargetEquipped(formID, instanceKey);
            const bool success = afterEquipped == shouldBeEquipped;
            const auto inventoryCount = InventoryCountForForm(
                RE::TESForm::GetFormByID<RE::TESBoundObject>(formID));
            if (success) {
                ++g_useCounts[g_weaponSwitchFavorite.formID];
            }
            Log("FAVORITE_ACTION_COMPLETE form=" + std::to_string(formID) +
                " action=" + (shouldBeEquipped ? "EquipWeapon" : "UnequipWeapon") +
                " rawResult=" + std::to_string(g_weaponSwitchRawResult ? 1 : 0) +
                " afterEquipped=" + std::to_string(afterEquipped ? 1 : 0) +
                " inventoryCountAfter=" + std::to_string(inventoryCount) +
                " success=" + std::to_string(success ? 1 : 0));
            g_weaponSwitchActive = false;
            g_weaponSwitchEquipIssued = false;
            g_weaponSwitchDrawRequested = false;
            g_weaponSwitchUnequip = false;
            g_weaponSwitchUnequipIssued = false;
            Log("weapon switch finished postEquipped=" + std::to_string(afterEquipped ? 1 : 0) +
                " success=" + std::to_string(success ? 1 : 0));
            FavoritesMenu::RefreshSnapshotIfOpen();
        }

        void ScheduleWeaponSwitch(std::uint32_t a_serial, std::uint32_t a_attempt);

        void ExecuteWeaponSwitch(std::uint32_t a_serial, std::uint32_t a_attempt)
        {
            if (!g_weaponSwitchActive || a_serial != g_weaponSwitchSerial) {
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(g_weaponSwitchFavorite.formID);
            if (!player || !object || !object->Is(RE::ENUM_FORM_ID::kWEAP)) {
                FinishWeaponSwitch(a_serial, false);
                return;
            }
            if (g_weaponSwitchUnequip) {
                if (!IsTargetEquipped(g_weaponSwitchFavorite.formID, g_weaponSwitchFavorite.instanceKey)) {
                    FinishWeaponSwitch(a_serial, true);
                    return;
                }
                if (!g_weaponSwitchUnequipIssued) {
                    const auto selected = FindInventoryStack(object, g_weaponSwitchFavorite.instanceKey);
                    if (!selected.item || !selected.stack) {
                        Log("weapon unequip skipped: inventory stack missing");
                        FinishWeaponSwitch(a_serial, false);
                        return;
                    }
                    if (auto* manager = RE::ActorEquipManager::GetSingleton()) {
                        RE::BGSObjectInstance instance(object,
                            selected.item->GetInstanceData(selected.stackID));
                        const bool result = manager->UnequipObject(
                            player, &instance, 1, nullptr, selected.stackID,
                            false, true, true, true, nullptr);
                        Log("weapon unequip request form=" +
                            std::to_string(object->GetFormID()) +
                            " stack=" + std::to_string(selected.stackID) +
                            " result=" + std::to_string(result ? 1 : 0));
                        g_weaponSwitchRawResult = result;
                        // The request was issued even when the engine returns
                        // false; the post-state decides whether it worked.
                        g_weaponSwitchUnequipIssued = true;
                    } else {
                        FinishWeaponSwitch(a_serial, false);
                        return;
                    }
                }
                if (g_weaponSwitchUnequipIssued && !IsTargetEquipped(g_weaponSwitchFavorite.formID, g_weaponSwitchFavorite.instanceKey)) {
                    FinishWeaponSwitch(a_serial, true);
                    return;
                }
                if (a_attempt < 30) {
                    ScheduleWeaponSwitch(a_serial, a_attempt + 1);
                } else {
                    FinishWeaponSwitch(a_serial, false);
                }
                return;
            }
            if (!g_weaponSwitchEquipIssued) {
                if (!IsTargetEquipped(g_weaponSwitchFavorite.formID, g_weaponSwitchFavorite.instanceKey)) {
                    const bool rawResult = EquipWeaponDirectOnce(object, g_weaponSwitchFavorite.instanceKey);
                    g_weaponSwitchRawResult = rawResult;
                    if (!rawResult) {
                        if (a_attempt < 30) {
                            ScheduleWeaponSwitch(a_serial, a_attempt + 1);
                        } else {
                            FinishWeaponSwitch(a_serial, false);
                        }
                        return;
                    }
                }
                g_weaponSwitchEquipIssued = true;
            }

            if (!IsTargetEquipped(g_weaponSwitchFavorite.formID, g_weaponSwitchFavorite.instanceKey)) {
                if (const auto mismatch = FindEquippedInstance(object, g_weaponSwitchFavorite.instanceKey);
                    mismatch.stack) {
                    Log("weapon switch mismatch: another instance is equipped; aborting target switch");
                    if (auto* manager = RE::ActorEquipManager::GetSingleton()) {
                        RE::BGSObjectInstance wrongInstance(object,
                            mismatch.item->GetInstanceData(mismatch.stackID));
                        manager->UnequipObject(RE::PlayerCharacter::GetSingleton(), &wrongInstance, 1,
                            nullptr, mismatch.stackID, false, true, true, true, nullptr);
                    }
                    FinishWeaponSwitch(a_serial, false);
                    return;
                }
                if (a_attempt < 30) {
                    ScheduleWeaponSwitch(a_serial, a_attempt + 1);
                } else {
                    FinishWeaponSwitch(a_serial, false);
                }
                return;
            }

            if (player->weaponState == RE::WEAPON_STATE::kDrawn) {
                FinishWeaponSwitch(a_serial, true);
                return;
            }
            if (player->weaponState == RE::WEAPON_STATE::kSheathed &&
                (!g_weaponSwitchDrawRequested || a_attempt % 4 == 0)) {
                if (!g_weaponSwitchDrawRequested && player->currentProcess) {
                    player->currentProcess->RequestLoadAnimationsForWeaponChange(*player);
                }
                g_weaponSwitchDrawRequested = true;
                player->SetWeaponState(RE::WEAPON_STATE::kWantToDraw);
                Log("weapon draw requested form=" + std::to_string(object->GetFormID()));
            }
            if (a_attempt < 30) {
                ScheduleWeaponSwitch(a_serial, a_attempt + 1);
            } else {
                FinishWeaponSwitch(a_serial, false);
            }
        }

        void ScheduleWeaponSwitch(std::uint32_t a_serial, std::uint32_t a_attempt)
        {
            std::thread([a_serial, a_attempt] {
                Sleep(30);
                if (auto* tasks = F4SE::GetTaskInterface()) {
                    tasks->AddTask([a_serial, a_attempt] {
                        std::lock_guard lock(g_mutex);
                        ExecuteWeaponSwitch(a_serial, a_attempt);
                    });
                }
            }).detach();
        }

        bool StartWeaponSwitch(std::uint32_t a_formID, std::uint64_t a_instanceKey)
        {
            auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID);
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!object || !player || !object->Is(RE::ENUM_FORM_ID::kWEAP)) {
                return false;
            }
            ++g_weaponSwitchSerial;
            g_weaponSwitchFavorite = { a_formID, a_instanceKey, QUICKKEY_NONE, 0 };
            g_weaponSwitchActive = true;
            g_weaponSwitchEquipIssued = false;
            g_weaponSwitchDrawRequested = false;
            g_weaponSwitchUnequip = IsTargetEquipped(a_formID, a_instanceKey);
            g_weaponSwitchUnequipIssued = false;
            g_weaponSwitchRawResult = false;
            ScheduleWeaponSwitch(g_weaponSwitchSerial, 0);
            return true;
        }

        void CompleteArmorAction(
            std::uint32_t a_formID,
            std::uint64_t a_instanceKey,
            bool a_shouldBeEquipped,
            bool a_rawResult,
            std::uint32_t a_attempt);

        void ScheduleArmorActionVerification(
            std::uint32_t a_formID,
            std::uint64_t a_instanceKey,
            bool a_shouldBeEquipped,
            bool a_rawResult,
            std::uint32_t a_attempt)
        {
            std::thread([a_formID, a_instanceKey, a_shouldBeEquipped, a_rawResult, a_attempt] {
                Sleep(50);
                if (auto* tasks = F4SE::GetTaskInterface()) {
                    tasks->AddTask([a_formID, a_instanceKey, a_shouldBeEquipped, a_rawResult, a_attempt] {
                        std::lock_guard lock(g_mutex);
                        CompleteArmorAction(a_formID, a_instanceKey, a_shouldBeEquipped, a_rawResult, a_attempt);
                    });
                }
            }).detach();
        }

        void CompleteArmorAction(
            std::uint32_t a_formID,
            std::uint64_t a_instanceKey,
            bool a_shouldBeEquipped,
            bool a_rawResult,
            std::uint32_t a_attempt)
        {
            auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID);
            if (!object) {
                return;
            }
            const bool afterEquipped = IsTargetEquipped(a_formID, a_instanceKey);
            const auto inventoryCount = InventoryCountForForm(object);
            const bool success = afterEquipped == a_shouldBeEquipped;
            if (!success && a_attempt < 20) {
                ScheduleArmorActionVerification(
                    a_formID, a_instanceKey, a_shouldBeEquipped, a_rawResult, a_attempt + 1);
                return;
            }
            Log("FAVORITE_ACTION_COMPLETE form=" + std::to_string(a_formID) +
                " action=" + (a_shouldBeEquipped ? "EquipArmor" : "UnequipArmor") +
                " rawResult=" + std::to_string(a_rawResult ? 1 : 0) +
                " afterEquipped=" + std::to_string(afterEquipped ? 1 : 0) +
                " inventoryCountAfter=" + std::to_string(inventoryCount) +
                " success=" + std::to_string(success ? 1 : 0));
            if (success) {
                ++g_useCounts[a_formID];
            }
            FavoritesMenu::RefreshSnapshotIfOpen();
        }

        void CompleteAlchemyAction(
            std::uint32_t a_formID,
            std::uint64_t a_instanceKey,
            std::uint32_t a_beforeCount,
            bool a_rawResult,
            std::uint32_t a_attempt)
        {
            auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID);
            if (!object) {
                return;
            }
            const auto afterCount = InventoryCountForForm(object);
            const bool success = afterCount < a_beforeCount;
            if (!success && a_attempt < 12) {
                std::thread([a_formID, a_instanceKey, a_beforeCount, a_rawResult, a_attempt] {
                    Sleep(50);
                    if (auto* tasks = F4SE::GetTaskInterface()) {
                        tasks->AddTask([a_formID, a_instanceKey, a_beforeCount, a_rawResult, a_attempt] {
                            std::lock_guard lock(g_mutex);
                            CompleteAlchemyAction(
                                a_formID, a_instanceKey, a_beforeCount, a_rawResult, a_attempt + 1);
                        });
                    }
                }).detach();
                return;
            }
            Log("FAVORITE_ACTION_COMPLETE form=" + std::to_string(a_formID) +
                " action=ConsumeAlchemy rawResult=" + std::to_string(a_rawResult ? 1 : 0) +
                " inventoryCountAfter=" + std::to_string(afterCount) +
                " success=" + std::to_string(success ? 1 : 0));
            if (success) {
                ++g_useCounts[a_formID];
            }
            FavoritesMenu::RefreshSnapshotIfOpen();
        }

        std::string NameFor(RE::BGSInventoryItem& a_item, std::uint32_t a_stackID)
        {
            auto* stack = a_item.stackData.get();
            for (std::uint32_t index = 0; stack && index < a_stackID; ++index) {
                stack = stack->nextStack.get();
            }
            const char* display = stack && stack->extra ?
                a_item.GetDisplayFullName(stack->extra.get()) :
                a_item.GetDisplayFullName(a_stackID);
            if (display && display[0]) {
                return display;
            }
            if (auto* fullName = a_item.object ? a_item.object->As<RE::TESFullName>() : nullptr) {
                if (const char* name = fullName->GetFullName(); name && name[0]) {
                    return name;
                }
            }
            return a_item.object ? a_item.object->GetFormTypeString() : "Unknown";
        }

        std::string TypeFor(RE::TESForm* a_form)
        {
            if (!a_form) {
                return "物品";
            }
            if (a_form->Is(RE::ENUM_FORM_ID::kWEAP)) {
                return "武器";
            }
            if (a_form->Is(RE::ENUM_FORM_ID::kARMO)) {
                return "服装";
            }
            if (a_form->Is(RE::ENUM_FORM_ID::kALCH)) {
                return "药品";
            }
            return "物品";
        }

        std::string FallbackIconCategory(RE::TESBoundObject* a_object,
            std::string_view a_name)
        {
            if (!a_object) {
                return "Utility";
            }
            std::string lowerName(a_name);
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                [](unsigned char ch) {
                    return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') :
                        static_cast<char>(ch);
                });
            const auto hasName = [&](std::initializer_list<std::string_view> words) {
                return std::any_of(words.begin(), words.end(), [&](std::string_view word) {
                    return lowerName.find(word) != std::string::npos;
                });
            };
            const auto* keywordForm = a_object->As<RE::BGSKeywordForm>();
            const auto hasKeyword = [&](std::initializer_list<std::string_view> keywords) {
                return keywordForm && std::any_of(keywords.begin(), keywords.end(),
                    [&](std::string_view keyword) { return keywordForm->HasKeywordString(keyword); });
            };

            // Mod workbench/menu entries are frequently ALCH or MISC forms.
            // Resolve them before the broad form-type buckets so they never
            // fall through to the medicine silhouette.
            if (hasName({ "工作台", "管理", "换装", "workbench", "workshop", "mod menu", "utility" })) {
                return "Utility";
            }

            if (a_object->Is(RE::ENUM_FORM_ID::kARMO)) {
                if (hasKeyword({ "ArmorTypeHelmet", "ArmorTypeHat", "ArmorTypeEyewear",
                    "ArmorTypeMask", "ArmorTypeHead" }) ||
                    hasName({ "helmet", "hat", "goggles", "glasses", "mask", "hood",
                        "头盔", "帽", "眼镜", "护目镜", "面具", "面罩" })) {
                    return "Headwear";
                }
                if (hasKeyword({ "ArmorTypeFoot", "ArmorTypeLeg" }) ||
                    hasName({ "shoe", "boot", "heel", "stocking", "sock", "legwear",
                        "鞋", "靴", "高跟", "袜", "长袜", "长裤", "裙",
                        "腿", "丝袜", "足" })) {
                    return "Footwear";
                }
                if (hasKeyword({ "ArmorTypeBody", "ArmorTypeChest", "ArmorTypeArmor" }) ||
                    hasName({ "armor", "armour", "护甲", "胸甲", "防弹" })) {
                    return "Armor";
                }
                return "Clothing";
            }

            if (a_object->Is(RE::ENUM_FORM_ID::kALCH)) {
                if (hasKeyword({ "VendorItemFood", "VendorItemDrink", "ObjectTypeFood" }) ||
                    hasName({ "food", "drink", "water", "alcohol", "soda", "nuka",
                        "food", "水", "饮料", "食物", "罐头", "啤酒", "酒" })) {
                    return "FoodDrink";
                }
                return "Medicine";
            }

            if (a_object->Is(RE::ENUM_FORM_ID::kWEAP)) {
                if (hasKeyword({ "ObjectTypeGrenade", "ObjectTypeMine", "WeaponTypeGrenade",
                    "WeaponTypeMine", "WeaponTypeThrown", "WeaponTypeExplosive" }) ||
                    hasName({ "grenade", "mine", "throwable", "手雷", "地雷", "投掷" })) {
                    return "Explosive";
                }
                if (hasKeyword({ "WeaponTypeMelee", "WeaponTypeMelee1H", "WeaponTypeMelee2H",
                    "WeapTypeMelee1H", "WeapTypeMelee2H" }) ||
                    hasName({ "melee", "unarmed", "刀", "剑", "棍", "锤", "拳套" })) {
                    return "Melee";
                }
                if (hasKeyword({ "WeaponTypePistol", "WeapTypePistol" }) ||
                    hasName({ "pistol", "revolver", "手枪", "左轮" })) {
                    return "Pistol";
                }
                if (hasKeyword({ "WeaponTypeHeavyGun", "WeapTypeHeavyGun", "WeaponTypeHeavy" }) ||
                    hasName({ "launcher", "gatling", "fatman", "fat man", "heavy", "火箭筒", "加特林" })) {
                    return "Heavy";
                }
                return "Rifle";
            }

            // Aozora Basic Icons intentionally keep non-combat inventory in
            // one small Utility bucket instead of inventing dozens of icons.
            return "Utility";
        }

        std::wstring Utf8ToWide(std::string_view a_text)
        {
            if (a_text.empty()) {
                return {};
            }
            const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                a_text.data(), static_cast<int>(a_text.size()), nullptr, 0);
            if (length <= 0) {
                return {};
            }
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a_text.data(),
                static_cast<int>(a_text.size()), result.data(), length);
            return result;
        }

        bool HasNonAscii(std::string_view a_text)
        {
            return std::any_of(a_text.begin(), a_text.end(), [](unsigned char a_char) {
                return a_char >= 0x80;
            });
        }

        int CompareLocalizedNames(std::string_view a_lhs, std::string_view a_rhs)
        {
            const auto lhs = Utf8ToWide(a_lhs);
            const auto rhs = Utf8ToWide(a_rhs);
            if (lhs.empty() || rhs.empty()) {
                const auto left = std::string(a_lhs);
                const auto right = std::string(a_rhs);
                return left.compare(right);
            }
            const wchar_t* locale = HasNonAscii(a_lhs) || HasNonAscii(a_rhs) ?
                L"zh-CN" : L"en-US";
            return CompareStringEx(locale, NORM_IGNORECASE,
                lhs.c_str(), static_cast<int>(lhs.size()),
                rhs.c_str(), static_cast<int>(rhs.size()), nullptr, nullptr, 0) - CSTR_EQUAL;
        }

        std::uint64_t PreferredInstanceKey(std::uint32_t a_formID)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return 0;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_formID) {
                    continue;
                }
                std::uint32_t stackID = 0;
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                    if (stack->extra && stack->extra->IsFavorite()) {
                        return InstanceKeyForStack(item, stackID);
                    }
                }
                stackID = 0;
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                    if (stack->IsEquipped()) {
                        return InstanceKeyForStack(item, stackID);
                    }
                }
                return InstanceKeyForStack(item, 0);
            }
            return 0;
        }

        void RebuildIndex()
        {
            g_index.clear();
            g_nextAddOrder = 1;
            for (std::size_t i = 0; i < g_favorites.size(); ++i) {
                g_index[{ g_favorites[i].formID, g_favorites[i].instanceKey }] = i;
                g_nextAddOrder = std::max(g_nextAddOrder, g_favorites[i].addOrder + 1);
            }
        }

        void ClearVanillaSlots()
        {
            if (auto* manager = RE::FavoritesManager::GetSingleton()) {
                for (auto& object : manager->storedFavTypes) {
                    object = nullptr;
                }
                manager->ClearCurrentAmmoCount();
            }
        }

        bool ClearFavoriteExtra(RE::ExtraDataList* a_extra)
        {
            if (!a_extra) {
                return false;
            }
            bool changed = false;
            if (a_extra->GetByType<RE::ExtraFavorite>()) {
                if (auto* favorite = a_extra->GetByType<RE::ExtraFavorite>()) {
                    favorite->quickkeyIndex = -1;
                }
                changed = a_extra->ClearFavorite() || changed;
            }
            if (a_extra->GetByType<RE::ExtraFavorite>()) {
                changed = static_cast<bool>(a_extra->RemoveExtra(RE::EXTRA_DATA_TYPE::kFavorite)) || changed;
            }
            if (a_extra->IsFavorite()) {
                changed = a_extra->ClearFavorite() || changed;
            }
            return changed;
        }

        void NotifyFavoriteChanged(RE::BGSInventoryItem* a_item)
        {
            if (!a_item) {
                return;
            }
            auto* inventory = RE::BGSInventoryInterface::GetSingleton();
            if (!inventory) {
                return;
            }
            auto* source = reinterpret_cast<RE::BSTEventSource<RE::InventoryInterface::FavoriteChangedEvent>*>(
                reinterpret_cast<std::uintptr_t>(inventory) + 0x60);
            RE::InventoryInterface::FavoriteChangedEvent event;
            event.itemAffected = a_item;
            source->Notify(event);
        }

        void QueueFavoriteMenusRefresh()
        {
            auto refresh = [] {
                if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                    queue->AddMessage(RE::BSFixedString("PipboyMenu"),
                        RE::UI_MESSAGE_TYPE::kInventoryUpdate);
                    queue->AddMessage(RE::BSFixedString("FavoritesMenu"),
                        RE::UI_MESSAGE_TYPE::kInventoryUpdate);
                }
            };
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddUITask(std::move(refresh));
            } else {
                refresh();
            }
        }

        void QueueFavoriteChangedNotifications(
            std::vector<std::uint32_t> a_formIDs)
        {
            if (a_formIDs.empty()) {
                return;
            }
            auto notify = [formIDs = std::move(a_formIDs)] {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player || !player->inventoryList) {
                    return;
                }
                const auto wasInternal = g_internalFavoriteUpdate.exchange(true);
                for (const auto formID : formIDs) {
                    for (auto& item : player->inventoryList->data) {
                        if (item.object && item.object->GetFormID() == formID) {
                            NotifyFavoriteChanged(std::addressof(item));
                            break;
                        }
                    }
                }
                g_internalFavoriteUpdate = wasInternal;
            };
            if (auto* tasks = F4SE::GetTaskInterface()) {
                // Run after the store mutation has released g_mutex. The
                // Pip-Boy data sink can then rebuild FavoritesList without
                // recursively entering our FavoriteChangedSink.
                tasks->AddTask(std::move(notify));
            } else {
                notify();
            }
        }

        void QueueDirectPipboyRefresh(std::uint32_t a_formID)
        {
            Log("PIPBOY_DIRECT_REFRESH form=" + std::to_string(a_formID) +
                " requested=1");
            auto refresh = [a_formID] {
                auto* player = RE::PlayerCharacter::GetSingleton();
                RE::BGSInventoryItem* affected = nullptr;
                if (player && player->inventoryList) {
                    for (auto& item : player->inventoryList->data) {
                        if (item.object && item.object->GetFormID() == a_formID) {
                            affected = std::addressof(item);
                            break;
                        }
                    }
                }
                if (!affected) {
                    Log("PIPBOY_DIRECT_REFRESH form=" + std::to_string(a_formID) +
                        " result=failed");
                    return;
                }
                const auto wasInternal = g_internalFavoriteUpdate.exchange(true);
                NotifyFavoriteChanged(affected);
                g_internalFavoriteUpdate = wasInternal;
                QueueFavoriteMenusRefresh();
                Log("PIPBOY_DIRECT_REFRESH form=" + std::to_string(a_formID) +
                    " result=success");
            };
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddTask(std::move(refresh));
            } else {
                refresh();
            }
        }

        void EnsureInitialized()
        {
            if (g_initialized) {
                return;
            }
            g_initialized = true;
        }

        StoredFavorite* Find(std::uint32_t a_formID, std::uint64_t a_instanceKey)
        {
            auto found = g_index.find({ a_formID, a_instanceKey });
            if (found != g_index.end() && found->second < g_favorites.size()) {
                return std::addressof(g_favorites[found->second]);
            }
            for (auto& entry : g_favorites) {
                if (entry.formID == a_formID &&
                    (a_instanceKey == 0 || entry.instanceKey == 0)) {
                    return std::addressof(entry);
                }
            }
            return nullptr;
        }

        StoredFavorite* FindExact(std::uint32_t a_formID, std::uint64_t a_instanceKey)
        {
            const auto found = g_index.find({ a_formID, a_instanceKey });
            if (found != g_index.end() && found->second < g_favorites.size()) {
                return std::addressof(g_favorites[found->second]);
            }
            return nullptr;
        }

        StoredFavorite* FindOnlyFavoriteForForm(std::uint32_t a_formID)
        {
            StoredFavorite* only = nullptr;
            for (auto& entry : g_favorites) {
                if (entry.formID != a_formID) {
                    continue;
                }
                if (only) {
                    // Do not turn a same-form multi-instance inventory into
                    // a guessed selection. The caller must use the game's
                    // exact resolver in that case.
                    return nullptr;
                }
                only = std::addressof(entry);
            }
            return only;
        }

        bool EnsureCustomFavoriteMarker(InventoryRef a_selected)
        {
            if (!a_selected.item || !a_selected.item->object || !a_selected.stack) {
                return false;
            }
            // A stack created by a weapon split can have no ExtraDataList yet.
            // Allocate it on the game thread before adding the marker so the
            // Pip-Boy can display the same favorite glyph without asking the
            // vanilla FavoritesManager to allocate a temporary slot.
            if (!a_selected.stack->extra) {
                a_selected.stack->extra.reset(new RE::ExtraDataList());
            }
            if (!a_selected.stack->extra) {
                return false;
            }
            if (auto* favorite = a_selected.stack->extra->GetByType<RE::ExtraFavorite>()) {
                favorite->quickkeyIndex = -1;
            } else {
                a_selected.stack->extra->SetFavorite(static_cast<char>(-1));
            }
            return a_selected.stack->extra->IsFavorite();
        }

        bool ClearCustomFavoriteMarker(InventoryRef a_selected)
        {
            return a_selected.stack && a_selected.stack->extra &&
                ClearFavoriteExtra(a_selected.stack->extra.get());
        }

        bool IsFavoriteMarkerPresent(InventoryRef a_selected)
        {
            return a_selected.stack && a_selected.stack->extra &&
                a_selected.stack->extra->IsFavorite();
        }

        InventoryRef FindStackByID(std::uint32_t a_formID, std::uint32_t a_stackID)
        {
            InventoryRef result;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return result;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_formID) {
                    continue;
                }
                auto* stack = item.stackData.get();
                for (std::uint32_t index = 0; stack && index < a_stackID; ++index) {
                    stack = stack->nextStack.get();
                }
                if (stack) {
                    result = { std::addressof(item), stack, a_stackID };
                }
                break;
            }
            return result;
        }

        bool HasMoreThanOneStack(std::uint32_t a_formID)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return false;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != a_formID) {
                    continue;
                }
                return item.stackData && item.stackData->nextStack;
            }
            return false;
        }

        InventoryRef FindMarkedStackForSlot(std::int8_t a_slot)
        {
            InventoryRef result;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return result;
            }
            for (auto& item : player->inventoryList->data) {
                if (!item.object) {
                    continue;
                }
                std::uint32_t stackID = 0;
                for (auto* stack = item.stackData.get(); stack;
                    stack = stack->nextStack.get(), ++stackID) {
                    const auto* favorite = stack->extra ?
                        stack->extra->GetByType<RE::ExtraFavorite>() : nullptr;
                    if (favorite && favorite->quickkeyIndex == a_slot) {
                        return { std::addressof(item), stack, stackID };
                    }
                }
            }
            return result;
        }

        StoredFavorite* FindFavoriteForSelectedStack(
            std::uint32_t a_formID,
            std::uint64_t a_instanceKey,
            InventoryRef a_selected)
        {
            if (auto* exact = FindExact(a_formID, a_instanceKey)) {
                return exact;
            }

            StoredFavorite* onlyFavorite = nullptr;
            for (auto& entry : g_favorites) {
                if (entry.formID != a_formID) {
                    continue;
                }
                if (onlyFavorite) {
                    // Multiple same-form favorites require the exact instance
                    // key. Never guess which modified weapon/armor was chosen.
                    return nullptr;
                }
                onlyFavorite = std::addressof(entry);
            }
            if (!onlyFavorite) {
                return nullptr;
            }

            const auto* marker = a_selected.stack && a_selected.stack->extra ?
                a_selected.stack->extra->GetByType<RE::ExtraFavorite>() : nullptr;
            const bool selectedIsCustomFavorite = marker && marker->quickkeyIndex == -1;
            if (!HasMoreThanOneStack(a_formID) || selectedIsCustomFavorite) {
                if (onlyFavorite->instanceKey != a_instanceKey) {
                    onlyFavorite->instanceKey = a_instanceKey;
                    RebuildIndex();
                    Log("PIPBOY identity repaired for unique selected stack form=" +
                        std::to_string(a_formID));
                }
                return onlyFavorite;
            }
            return nullptr;
        }

        bool SetPipboyQuickkeyForSelection(
            std::uint32_t a_selectedIndex,
            std::int32_t a_slot)
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->GetMenuOpen<RE::PipboyMenu>()) {
                return false;
            }
            auto pipboy = ui->GetMenu<RE::PipboyMenu>();
            if (!pipboy) {
                return false;
            }
            // This is the game's own selected-row resolver. We call it only
            // after the native Pip-Boy hook has consumed the Q/Favorite event;
            // no vanilla favorites page is shown and the resulting slot is
            // cleared immediately below.
            ExtendInternalFavoriteUpdate(250);
            const auto wasInternal = g_internalFavoriteUpdate.exchange(true);
            pipboy->inventoryMenuObj.SetQuickkey(
                static_cast<int>(a_selectedIndex), static_cast<int>(a_slot));
            g_internalFavoriteUpdate = wasInternal;
            return true;
        }

        void UpgradeLegacyInstanceKeysUnlocked()
        {
            // FAV2 v1 records only carried a form ID. Resolve those records
            // once, while the inventory and its ExtraFavorite markers are
            // available, so later activation can require an exact identity.
            std::vector<StoredFavorite> splitEntries;
            bool changed = false;
            for (auto& legacy : g_favorites) {
                if (legacy.instanceKey != 0) {
                    continue;
                }
                auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(legacy.formID);
                if (!object || (!object->Is(RE::ENUM_FORM_ID::kWEAP) &&
                    !object->Is(RE::ENUM_FORM_ID::kARMO))) {
                    continue;
                }

                std::vector<std::uint64_t> marked;
                if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->inventoryList) {
                    for (auto& item : player->inventoryList->data) {
                        if (!item.object || item.object->GetFormID() != legacy.formID) {
                            continue;
                        }
                        std::uint32_t stackID = 0;
                        for (auto* stack = item.stackData.get(); stack;
                            stack = stack->nextStack.get(), ++stackID) {
                            const auto* favorite = stack->extra ?
                                stack->extra->GetByType<RE::ExtraFavorite>() : nullptr;
                            if (!favorite || favorite->quickkeyIndex != -1) {
                                continue;
                            }
                            const auto key = InstanceKeyForStack(item, stackID);
                            if (std::find(marked.begin(), marked.end(), key) == marked.end()) {
                                marked.push_back(key);
                            }
                        }
                    }
                }

                if (!marked.empty()) {
                    legacy.instanceKey = marked.front();
                    changed = true;
                    for (std::size_t index = 1; index < marked.size(); ++index) {
                        auto split = legacy;
                        split.instanceKey = marked[index];
                        split.hotkeySlot = QUICKKEY_NONE;
                        split.addOrder = g_nextAddOrder++;
                        splitEntries.push_back(split);
                    }
                } else if (const auto preferred = PreferredInstanceKey(legacy.formID); preferred != 0) {
                    legacy.instanceKey = preferred;
                    changed = true;
                }
            }
            if (!splitEntries.empty()) {
                g_favorites.insert(g_favorites.end(), splitEntries.begin(), splitEntries.end());
                changed = true;
            }
            if (changed) {
                RebuildIndex();
                Log("LEGACY_INSTANCE_UPGRADE resolved=" + std::to_string(g_favorites.size()));
            }
        }

        void MigrateLegacyMarkersUnlocked()
        {
            if (g_legacyMarkerMigrationAttempted || !g_favorites.empty()) {
                return;
            }
            g_legacyMarkerMigrationAttempted = true;
            std::uint32_t migrated = 0;
            if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->inventoryList) {
                for (auto& item : player->inventoryList->data) {
                    if (!item.object) {
                        continue;
                    }
                    std::uint32_t stackID = 0;
                    for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                        auto* favorite = stack->extra ? stack->extra->GetByType<RE::ExtraFavorite>() : nullptr;
                        // Prisma 2.x normalized its custom entries to -1 after
                        // detaching them from the vanilla 12-slot manager.
                        // Vanilla slots retain a concrete 0..11 index and are
                        // deliberately discarded instead of imported.
                        if (!favorite || favorite->quickkeyIndex != -1) {
                            continue;
                        }
                        const Identity identity{ item.object->GetFormID(), InstanceKeyForStack(item, stackID) };
                        if (g_index.find(identity) == g_index.end()) {
                            g_index[identity] = g_favorites.size();
                            g_favorites.push_back({ item.object->GetFormID(), identity.instanceKey,
                                QUICKKEY_NONE, g_nextAddOrder++ });
                            ++migrated;
                        }
                    }
                }
            }
            if (migrated) {
                RebuildIndex();
                Log("LEGACY_MARKER_MIGRATION migrated=" + std::to_string(migrated));
            } else {
                Log("LEGACY_MARKER_MIGRATION migrated=0");
            }
        }

        void ClearVanillaFavoritesUnlocked()
        {
            ExtendInternalFavoriteUpdate();
            const auto wasInternal = g_internalFavoriteUpdate.exchange(true);
            std::uint32_t clearedMarkers = 0;
            std::uint32_t restoredCustomMarkers = 0;
            std::vector<RE::BGSInventoryItem*> changedItems;
            if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->inventoryList) {
                for (auto& item : player->inventoryList->data) {
                    if (!item.object) {
                        continue;
                    }
                    std::uint32_t stackID = 0;
                    for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                        const auto key = InstanceKeyForStack(item, stackID);
                        auto* stored = key != 0 ? FindExact(item.object->GetFormID(), key) :
                            Find(item.object->GetFormID(), key);
                        if (stored) {
                            if (!stack->extra) {
                                stack->extra.reset(new RE::ExtraDataList());
                                if (!stack->extra) {
                                    continue;
                                }
                            }
                            auto* favorite = stack->extra->GetByType<RE::ExtraFavorite>();
                            bool changed = false;
                            if (!favorite) {
                                stack->extra->SetFavorite(static_cast<char>(-1));
                                changed = true;
                            } else if (favorite->quickkeyIndex != -1) {
                                favorite->quickkeyIndex = -1;
                                changed = true;
                            }
                            ++restoredCustomMarkers;
                            if (changed && std::find(changedItems.begin(), changedItems.end(), &item) == changedItems.end()) {
                                changedItems.push_back(&item);
                            }
                        } else if (stack->extra && stack->extra->GetByType<RE::ExtraFavorite>() &&
                            ClearFavoriteExtra(stack->extra.get())) {
                            ++clearedMarkers;
                            if (std::find(changedItems.begin(), changedItems.end(), &item) == changedItems.end()) {
                                changedItems.push_back(&item);
                            }
                        }
                    }
                }
            }
            g_internalFavoriteUpdate = wasInternal;
            ClearVanillaSlots();
            std::vector<std::uint32_t> changedForms;
            changedForms.reserve(changedItems.size());
            for (auto* item : changedItems) {
                if (!item || !item->object) {
                    continue;
                }
                const auto formID = item->object->GetFormID();
                if (std::find(changedForms.begin(), changedForms.end(), formID) == changedForms.end()) {
                    changedForms.push_back(formID);
                }
            }
            QueueFavoriteChangedNotifications(std::move(changedForms));
            // Always refresh both vanilla views after clearing. A marker can
            // already be normalized to -1 by the favorite-change sink while
            // FavoritesManager still holds a stale object pointer; relying
            // only on changedItems would leave the old wheel icon visible.
            QueueFavoriteMenusRefresh();
            Log("vanilla favorites cleared markers=" + std::to_string(clearedMarkers) +
                " restored-custom-markers=" + std::to_string(restoredCustomMarkers) +
                " refreshed-items=" + std::to_string(changedItems.size()));
        }

        class FavoriteChangedSink final :
            public RE::BSTEventSink<RE::InventoryInterface::FavoriteChangedEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::InventoryInterface::FavoriteChangedEvent& a_event,
                RE::BSTEventSource<RE::InventoryInterface::FavoriteChangedEvent>*) override
            {
                if (!a_event.itemAffected || !a_event.itemAffected->object) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto formID = a_event.itemAffected->object->GetFormID();
                if (g_internalFavoriteUpdate.load() ||
                    GetTickCount64() < g_internalFavoriteUpdateUntil.load()) {
                    Log("PIPBOY_FAVORITE_EVENT ignored=internal form=" + std::to_string(formID));
                    return RE::BSEventNotifyControl::kContinue;
                }
                // Outside an active transaction the Aozora store is the
                // authority. A vanilla change must not create/remove store
                // entries or assign a permanent hotkey. Reapply the Store
                // state only as a display mirror, so a vanilla number key
                // cannot leave a stale marker in the Pip-Boy.
                if (auto* ui = RE::UI::GetSingleton(); ui &&
                    ui->GetMenuOpen(RE::BSFixedString("PipboyMenu"))) {
                    Log("PIPBOY_QUICKKEY_LEAK form=" + std::to_string(formID));
                }
                Log("PIPBOY_FAVORITE_EVENT ignored=outside-transaction form=" +
                    std::to_string(formID) + " action=display-mirror");
                if (auto* tasks = F4SE::GetTaskInterface()) {
                    tasks->AddTask([] { ClearVanillaFavorites(); });
                } else {
                    ClearVanillaFavorites();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        FavoriteChangedSink g_favoriteChangedSink;
        bool g_favoriteChangedSinkRegistered{ false };

        void SetTemporaryFavoriteMarker(RE::TESBoundObject* a_object, std::uint64_t a_instanceKey,
            std::uint32_t a_slot)
        {
            if (!a_object || a_slot >= 12) {
                return;
            }
            const auto selected = FindInventoryStack(a_object, a_instanceKey);
            if (!selected.stack) {
                return;
            }
            if (!selected.stack->extra) {
                selected.stack->extra.reset(new RE::ExtraDataList());
            }
            if (!selected.stack->extra) {
                return;
            }
            if (auto* favorite = selected.stack->extra->GetByType<RE::ExtraFavorite>()) {
                favorite->quickkeyIndex = static_cast<std::int8_t>(a_slot);
            } else {
                selected.stack->extra->SetFavorite(static_cast<char>(a_slot));
            }
        }

        InventoryRef ResolvePipboyFavoriteSelection(std::uint32_t a_selectedIndex)
        {
            std::uint32_t formID = 0;
            std::uint32_t stackID = 0xFFFFFFFFu;
            const bool nativeResolved = ResolvePipboySelectionByIndex(
                a_selectedIndex, formID, stackID);
            InventoryRef selected;
            if (nativeResolved) {
                selected = FindStackByID(formID, stackID);
            }
            if ((!selected.item || !selected.stack) && formID != 0) {
                if (auto* only = FindOnlyFavoriteForForm(formID); only) {
                    selected = FindInventoryStack(
                        RE::TESForm::GetFormByID<RE::TESBoundObject>(formID),
                        only->instanceKey);
                } else if (!HasMoreThanOneStack(formID)) {
                    selected = FindStackByID(formID, 0);
                }
            }
            if (selected.item && selected.stack && selected.item->object) {
                return selected;
            }

            // Last-resort exact resolver for new/split items. This probe is
            // internal and is cleared before the Store operation; it never
            // opens or delegates to the vanilla FavoritesMenu.
            if (!SetPipboyQuickkeyForSelection(a_selectedIndex, 0)) {
                Log("PIPBOY_Q resolve-failed=selection-unavailable");
                return {};
            }
            selected = FindMarkedStackForSlot(0);
            if (!selected.item || !selected.stack || !selected.item->object) {
                ClearVanillaSlots();
                Log("PIPBOY_Q resolve-failed=probe-marker-missing");
                return {};
            }
            const auto wasInternal = g_internalFavoriteUpdate.exchange(true);
            ClearFavoriteExtra(selected.stack->extra.get());
            if (auto* manager = RE::FavoritesManager::GetSingleton();
                manager && manager->storedFavTypes[0] == selected.item->object) {
                manager->storedFavTypes[0] = nullptr;
                manager->ClearCurrentAmmoCount();
            }
            g_internalFavoriteUpdate = wasInternal;
            return selected;
        }

        void ClearTemporarySlot(std::uint32_t a_slot, RE::TESBoundObject* a_object)
        {
            std::thread([a_slot, a_object] {
                Sleep(600);
                if (auto* tasks = F4SE::GetTaskInterface()) {
                    tasks->AddTask([a_slot, a_object] {
                        std::lock_guard lock(g_mutex);
                        auto* manager = RE::FavoritesManager::GetSingleton();
                        if (manager && a_slot < 12 &&
                            manager->storedFavTypes[a_slot] == a_object) {
                            manager->storedFavTypes[a_slot] = nullptr;
                            manager->ClearCurrentAmmoCount();
                        }
                        ClearVanillaFavoritesUnlocked();
                    });
                }
            }).detach();
        }
    }

    void InitializeStore()
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        ClearVanillaFavoritesUnlocked();
    }

    void RefreshStore()
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
    }

    void ClearVanillaFavorites()
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        MigrateLegacyMarkersUnlocked();
        ClearVanillaFavoritesUnlocked();
    }

    void RegisterFavoriteChangeSink()
    {
        std::lock_guard lock(g_mutex);
        if (g_favoriteChangedSinkRegistered) {
            return;
        }
        auto* inventory = RE::BGSInventoryInterface::GetSingleton();
        if (!inventory) {
            Log("favorite change sink waiting for inventory interface");
            return;
        }
        auto* source = reinterpret_cast<RE::BSTEventSource<RE::InventoryInterface::FavoriteChangedEvent>*>(
            reinterpret_cast<std::uintptr_t>(inventory) + 0x60);
        source->RegisterSink(std::addressof(g_favoriteChangedSink));
        g_favoriteChangedSinkRegistered = true;
        Log("favorite change sink registered for independent store sync");
    }

    std::vector<FavoriteSnapshotEntry> Snapshot()
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        std::vector<FavoriteSnapshotEntry> result;
        result.reserve(g_favorites.size());
        std::sort(g_favorites.begin(), g_favorites.end(),
            [](const StoredFavorite& a_lhs, const StoredFavorite& a_rhs) {
                return a_lhs.addOrder < a_rhs.addOrder;
            });
        RebuildIndex();
        std::unordered_map<std::uint32_t, std::uint32_t> favoriteCounts;
        for (const auto& stored : g_favorites) {
            ++favoriteCounts[stored.formID];
        }
        bool instanceKeysRepaired = false;
        std::size_t equippedCount = 0;
        for (auto& stored : g_favorites) {
            auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(stored.formID);
            if (!object) {
                continue;
            }
            FavoriteSnapshotEntry entry;
            entry.formID = stored.formID;
            entry.instanceKey = stored.instanceKey;
            entry.hotkeySlot = stored.hotkeySlot;
            entry.type = TypeFor(object);
            entry.name = object->As<RE::TESFullName>() && object->As<RE::TESFullName>()->GetFullName() ?
                object->As<RE::TESFullName>()->GetFullName() : "Unknown";
            auto selected = FindInventoryStack(object, stored.instanceKey);
            if (selected.item && selected.stack) {
                entry.name = NameFor(*selected.item, selected.stackID);
                entry.count = selected.stack->GetCount();
                if (object->Is(RE::ENUM_FORM_ID::kWEAP) || object->Is(RE::ENUM_FORM_ID::kARMO)) {
                    const auto currentKey = InstanceKeyForStack(*selected.item, selected.stackID);
                    if (currentKey != 0 && stored.instanceKey != currentKey) {
                        stored.instanceKey = currentKey;
                        entry.instanceKey = currentKey;
                        instanceKeysRepaired = true;
                    }
                }
            }
            entry.iconCategory = FallbackIconCategory(object, entry.name);
            auto fisIcon = FISIconForName(entry.name);
            if (fisIcon.second.empty()) {
                fisIcon = FISIconForAutoTag(entry.name, AutoTagSectionForObject(object));
            }
            if (fisIcon.second.empty()) {
                fisIcon = FISIconForObject(object);
            }
            entry.iconLibrary = fisIcon.first;
            entry.iconClass = fisIcon.second;
            const auto iconSignature = entry.iconLibrary + "|" + entry.iconClass;
            if (g_lastLoggedFisIcons[stored.formID] != iconSignature) {
                g_lastLoggedFisIcons[stored.formID] = iconSignature;
                Log("FIS_ICON form=" + std::to_string(stored.formID) +
                    " library=" + (entry.iconLibrary.empty() ? "none" : entry.iconLibrary) +
                    " class=" + (entry.iconClass.empty() ? "none" : entry.iconClass));
            }
            const bool uniqueFavoriteForForm = favoriteCounts[stored.formID] == 1;
            entry.equipped = IsEquippedForIdentity(object, stored.instanceKey, uniqueFavoriteForForm);
            const auto equipSignature = std::to_string(stored.instanceKey) + "|" +
                (selected.stack ? std::to_string(selected.stackID) : "none") + "|" +
                (entry.equipped ? "1" : "0");
            if (g_lastLoggedEquipMatches[stored.formID] != equipSignature) {
                g_lastLoggedEquipMatches[stored.formID] = equipSignature;
                Log("EQUIP_MATCH form=" + std::to_string(stored.formID) +
                    " key=" + std::to_string(stored.instanceKey) +
                    " stack=" + (selected.stack ? std::to_string(selected.stackID) : "none") +
                    " equipped=" + (entry.equipped ? "1" : "0") +
                    " uniqueFallback=" + (uniqueFavoriteForForm ? "1" : "0"));
            }
            if (entry.equipped) {
                ++equippedCount;
            }
            entry.useCount = g_useCounts[stored.formID];
            result.push_back(std::move(entry));
        }
        if (instanceKeysRepaired) {
            RebuildIndex();
            Log("INSTANCE_KEYS repaired from current inventory stack");
        }
        static std::size_t lastEquippedCount = static_cast<std::size_t>(-1);
        if (equippedCount != lastEquippedCount) {
            lastEquippedCount = equippedCount;
            Log("SNAPSHOT_EQUIPPED count=" + std::to_string(equippedCount));
        }
        const auto sortMode = ReadAllSortMode();
        std::sort(result.begin(), result.end(), [sortMode](const FavoriteSnapshotEntry& a_lhs,
            const FavoriteSnapshotEntry& a_rhs) {
            if (sortMode == 1 && a_lhs.useCount != a_rhs.useCount) {
                return a_lhs.useCount > a_rhs.useCount;
            }
            if (a_lhs.name != a_rhs.name) {
                return CompareLocalizedNames(a_lhs.name, a_rhs.name) < 0;
            }
            if (a_lhs.formID != a_rhs.formID) {
                return a_lhs.formID < a_rhs.formID;
            }
            return a_lhs.instanceKey < a_rhs.instanceKey;
        });
        return result;
    }

    bool AddFavorite(std::uint32_t a_formID, std::uint64_t a_instanceKey, std::uint32_t a_hotkeySlot)
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        if (a_formID == 0) {
            return false;
        }
        if (a_instanceKey == 0) {
            a_instanceKey = PreferredInstanceKey(a_formID);
        }
        if ((a_instanceKey != 0 ? FindExact(a_formID, a_instanceKey) :
            Find(a_formID, a_instanceKey))) {
            EnsureCustomFavoriteMarker(FindInventoryStack(
                RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID), a_instanceKey));
            return false;
        }
        if (a_hotkeySlot >= 12) {
            a_hotkeySlot = QUICKKEY_NONE;
        }
        g_index[{ a_formID, a_instanceKey }] = g_favorites.size();
        g_favorites.push_back({ a_formID, a_instanceKey, a_hotkeySlot, g_nextAddOrder++ });
        EnsureCustomFavoriteMarker(FindInventoryStack(
            RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID), a_instanceKey));
        ClearVanillaFavoritesUnlocked();
        return true;
    }

    bool ToggleFavoriteForStack(std::uint32_t a_formID, std::uint32_t a_stackID)
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        if (a_stackID == 0xFFFFFFFFu) {
            if (HasMoreThanOneStack(a_formID)) {
                Log("direct Pip-Boy favorite skipped: stack identity unavailable for split inventory form=" +
                    std::to_string(a_formID));
                return false;
            }
            a_stackID = 0;
        }
        auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID);
        if (!object) {
            return false;
        }
        const auto selected = FindStackByID(a_formID, a_stackID);
        if (!selected.item || !selected.stack) {
            Log("direct Pip-Boy favorite skipped: selected stack missing form=" +
                std::to_string(a_formID) + " stack=" + std::to_string(a_stackID));
            return false;
        }
        const auto instanceKey = InstanceKeyForStack(*selected.item, a_stackID);
        if (auto* existing = FindFavoriteForSelectedStack(a_formID, instanceKey, selected)) {
            g_favorites.erase(std::remove_if(g_favorites.begin(), g_favorites.end(),
                [&](const StoredFavorite& a_entry) {
                    return a_entry.formID == existing->formID &&
                        a_entry.instanceKey == existing->instanceKey;
                }), g_favorites.end());
            RebuildIndex();
            ClearCustomFavoriteMarker(selected);
            ClearVanillaFavoritesUnlocked();
            QueueDirectPipboyRefresh(a_formID);
            Log("direct Pip-Boy favorite removed form=" + std::to_string(a_formID) +
                " stack=" + std::to_string(a_stackID));
            return true;
        }
        if (!EnsureCustomFavoriteMarker(selected)) {
            Log("direct Pip-Boy favorite skipped: marker allocation failed form=" +
                std::to_string(a_formID) + " stack=" + std::to_string(a_stackID));
            return false;
        }
        g_index[{ a_formID, instanceKey }] = g_favorites.size();
        g_favorites.push_back({ a_formID, instanceKey, QUICKKEY_NONE, g_nextAddOrder++ });
        ClearVanillaFavoritesUnlocked();
        QueueDirectPipboyRefresh(a_formID);
        Log("direct Pip-Boy favorite added form=" + std::to_string(a_formID) +
            " stack=" + std::to_string(a_stackID));
        return true;
    }

    bool ToggleFavoriteForPipboyIndex(std::uint32_t a_selectedIndex)
    {
        const auto selected = ResolvePipboyFavoriteSelection(a_selectedIndex);
        if (!selected.item || !selected.stack || !selected.item->object) {
            return false;
        }
        const auto formID = selected.item->object->GetFormID();
        const auto instanceKey = InstanceKeyForStack(*selected.item, selected.stackID);
        const bool wasFavorite = FindFavoriteForSelectedStack(
            formID, instanceKey, selected) != nullptr;
        Log("PIPBOY_NATIVE_INPUT selection form=" + std::to_string(formID) +
            " instance=" + std::to_string(instanceKey) +
            " action=" + (wasFavorite ? "remove" : "add"));
        const bool handled = ToggleFavoriteForStack(formID, selected.stackID);
        Log("PIPBOY_NATIVE_INPUT action-result=" +
            std::to_string(handled ? 1 : 0));
        return handled;
    }

    bool RemoveFavorite(std::uint32_t a_formID, std::uint64_t a_instanceKey)
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        auto* target = a_instanceKey != 0 ? FindExact(a_formID, a_instanceKey) :
            Find(a_formID, a_instanceKey);
        if (!target) {
            return false;
        }
        const auto targetFormID = target->formID;
        const auto targetInstanceKey = target->instanceKey;
        g_favorites.erase(std::remove_if(g_favorites.begin(), g_favorites.end(),
            [&](const StoredFavorite& a_entry) {
                return a_entry.formID == targetFormID &&
                    (a_instanceKey == 0 || a_entry.instanceKey == targetInstanceKey);
            }), g_favorites.end());
        RebuildIndex();
        ClearVanillaFavoritesUnlocked();
        return true;
    }

    bool AssignHotkey(std::uint32_t a_formID, std::uint64_t a_instanceKey, std::uint32_t a_hotkeySlot)
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        if (a_hotkeySlot >= 12) {
            return false;
        }
        auto* target = a_instanceKey != 0 ? FindExact(a_formID, a_instanceKey) :
            Find(a_formID, a_instanceKey);
        if (!target) {
            return false;
        }
        // Pressing the same number key on the same selected item toggles its
        // assignment off. Capture this before clearing the slot from every
        // other entry; otherwise the clear pass would erase the evidence and
        // immediately assign the key again.
        const bool removeExisting = target->hotkeySlot == a_hotkeySlot;
        for (auto& entry : g_favorites) {
            if (entry.hotkeySlot == a_hotkeySlot) {
                entry.hotkeySlot = QUICKKEY_NONE;
            }
        }
        if (!removeExisting) {
            target->hotkeySlot = a_hotkeySlot;
        }
        return true;
    }

    bool ExecuteFavoriteAction(
        std::uint32_t a_formID,
        std::uint64_t a_instanceKey,
        std::string_view a_source)
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        UpgradeLegacyInstanceKeysUnlocked();
        MigrateLegacyMarkersUnlocked();
        auto* stored = a_instanceKey != 0 ? FindExact(a_formID, a_instanceKey) :
            Find(a_formID, a_instanceKey);
        auto* object = RE::TESForm::GetFormByID<RE::TESBoundObject>(a_formID);
        if (!stored || !object) {
            return false;
        }
        auto selected = FindInventoryStack(object, stored->instanceKey);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !selected.item || !selected.stack) {
            Log("FAVORITE_ACTION source=" + std::string(a_source) +
                " form=" + std::to_string(a_formID) +
                " action=Unsupported accepted=0 reason=inventory-stack-missing");
            return false;
        }

        const auto inventoryCount = InventoryCountForForm(object);
        const bool isEquipment = object->Is(RE::ENUM_FORM_ID::kWEAP) ||
            object->Is(RE::ENUM_FORM_ID::kARMO);
        const bool beforeEquipped = isEquipment ?
            IsTargetEquipped(a_formID, stored->instanceKey) : false;
        const std::string formType = object->Is(RE::ENUM_FORM_ID::kWEAP) ? "WEAP" :
            (object->Is(RE::ENUM_FORM_ID::kARMO) ? "ARMO" :
                (object->Is(RE::ENUM_FORM_ID::kALCH) ? "ALCH" : "OTHER"));

        if (object->Is(RE::ENUM_FORM_ID::kWEAP)) {
            const std::string action = beforeEquipped ? "UnequipWeapon" : "EquipWeapon";
            Log("FAVORITE_ACTION source=" + std::string(a_source) +
                " form=" + std::to_string(a_formID) +
                " instanceKey=" + std::to_string(stored->instanceKey) +
                " formType=" + formType +
                " action=" + action +
                " beforeEquipped=" + std::to_string(beforeEquipped ? 1 : 0) +
                " inventoryCount=" + std::to_string(inventoryCount));
            if (StartWeaponSwitch(a_formID, stored->instanceKey)) {
                Log("FAVORITE_ACTION accepted=1 form=" + std::to_string(a_formID) +
                    " action=" + action);
                return true;
            }
            Log("FAVORITE_ACTION accepted=0 form=" + std::to_string(a_formID) +
                " action=" + action);
            return false;
        }

        if (object->Is(RE::ENUM_FORM_ID::kARMO)) {
            const std::string action = beforeEquipped ? "UnequipArmor" : "EquipArmor";
            Log("FAVORITE_ACTION source=" + std::string(a_source) +
                " form=" + std::to_string(a_formID) +
                " instanceKey=" + std::to_string(stored->instanceKey) +
                " formType=" + formType +
                " action=" + action +
                " beforeEquipped=" + std::to_string(beforeEquipped ? 1 : 0) +
                " inventoryCount=" + std::to_string(inventoryCount));
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) {
                Log("FAVORITE_ACTION accepted=0 form=" + std::to_string(a_formID) +
                    " action=" + action + " reason=equip-manager-missing");
                return false;
            }
            RE::BGSObjectInstance instance(object, selected.item->GetInstanceData(selected.stackID));
            const bool rawResult = beforeEquipped ?
                equipManager->UnequipObject(player, &instance, 1, nullptr, selected.stackID,
                    false, true, true, true, nullptr) :
                equipManager->EquipObject(player, instance, selected.stackID, 1, nullptr,
                    false, true, true, true, false);
            Log("FAVORITE_ACTION accepted=1 form=" + std::to_string(a_formID) +
                " action=" + action +
                " rawResult=" + std::to_string(rawResult ? 1 : 0));
            ScheduleArmorActionVerification(
                a_formID, stored->instanceKey, !beforeEquipped, rawResult, 0);
            return true;
        }

        if (object->Is(RE::ENUM_FORM_ID::kALCH)) {
            auto* alchemy = object->As<RE::AlchemyItem>();
            if (!alchemy) {
                Log("FAVORITE_ACTION source=" + std::string(a_source) +
                    " form=" + std::to_string(a_formID) +
                    " formType=ALCH action=Unsupported accepted=0 reason=alchemy-cast-failed");
                return false;
            }
            Log("FAVORITE_ACTION source=" + std::string(a_source) +
                " form=" + std::to_string(a_formID) +
                " instanceKey=" + std::to_string(stored->instanceKey) +
                " formType=ALCH action=ConsumeAlchemy beforeEquipped=0 inventoryCount=" +
                std::to_string(inventoryCount));
            const bool rawResult = player->DrinkPotion(alchemy, selected.stackID);
            Log("FAVORITE_ACTION accepted=1 form=" + std::to_string(a_formID) +
                " action=ConsumeAlchemy rawResult=" + std::to_string(rawResult ? 1 : 0));
            CompleteAlchemyAction(a_formID, stored->instanceKey, inventoryCount, rawResult, 0);
            return true;
        }

        Log("FAVORITE_ACTION source=" + std::string(a_source) +
            " form=" + std::to_string(a_formID) +
            " instanceKey=" + std::to_string(stored->instanceKey) +
            " formType=" + formType +
            " action=Unsupported beforeEquipped=0 inventoryCount=" +
            std::to_string(inventoryCount) + " accepted=0");
        return false;
    }

    bool ActivateFavorite(std::uint32_t a_formID, std::uint64_t a_instanceKey)
    {
        return ExecuteFavoriteAction(a_formID, a_instanceKey, "menu");
    }

    bool HasHotkeySlot(std::uint32_t a_hotkeySlot)
    {
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        return a_hotkeySlot < 12 && std::any_of(g_favorites.begin(), g_favorites.end(),
            [a_hotkeySlot](const StoredFavorite& a_entry) {
                return a_entry.hotkeySlot == a_hotkeySlot;
            });
    }

    bool ActivateHotkeySlot(std::uint32_t a_hotkeySlot)
    {
        std::uint32_t formID = 0;
        std::uint64_t instanceKey = 0;
        {
            std::lock_guard lock(g_mutex);
            EnsureInitialized();
            if (a_hotkeySlot >= 12) {
                return false;
            }
            for (const auto& entry : g_favorites) {
                if (entry.hotkeySlot == a_hotkeySlot) {
                    formID = entry.formID;
                    instanceKey = entry.instanceKey;
                    break;
                }
            }
        }
        return formID != 0 && ExecuteFavoriteAction(formID, instanceKey, "hotkey");
    }

    void Save(const F4SE::SerializationInterface* a_interface)
    {
        if (!a_interface) {
            return;
        }
        std::lock_guard lock(g_mutex);
        EnsureInitialized();
        if (a_interface->OpenRecord(USAGE_RECORD, USAGE_RECORD_VERSION)) {
            const auto count = static_cast<std::uint32_t>(g_useCounts.size());
            a_interface->WriteRecordData(count);
            for (const auto& [formID, useCount] : g_useCounts) {
                a_interface->WriteRecordData(formID);
                a_interface->WriteRecordData(useCount);
            }
        }
        if (a_interface->OpenRecord(FAVORITES_V2_RECORD, FAVORITES_V2_RECORD_VERSION)) {
            const auto count = static_cast<std::uint32_t>(g_favorites.size());
            a_interface->WriteRecordData(count);
            for (const auto& favorite : g_favorites) {
                a_interface->WriteRecordData(favorite.formID);
                a_interface->WriteRecordData(favorite.instanceKey);
                a_interface->WriteRecordData(favorite.hotkeySlot);
                a_interface->WriteRecordData(favorite.addOrder);
            }
        }
    }

    void Load(const F4SE::SerializationInterface* a_interface)
    {
        std::lock_guard lock(g_mutex);
        g_useCounts.clear();
        g_favorites.clear();
        g_index.clear();
        g_nextAddOrder = 1;
        g_initialized = false;
        g_legacyMarkerMigrationAttempted = false;
        Log("load callback begin");
        if (!a_interface) {
            Log("load callback missing serialization interface");
            return;
        }
        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;
        while (a_interface->GetNextRecordInfo(type, version, length)) {
            if (type == USAGE_RECORD && version == USAGE_RECORD_VERSION) {
                std::uint32_t count = 0;
                if (a_interface->ReadRecordData(count) != sizeof(count)) {
                    continue;
                }
                Log("loaded usage record count=" + std::to_string(count));
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::uint32_t oldFormID = 0;
                    std::uint32_t useCount = 0;
                    if (a_interface->ReadRecordData(oldFormID) != sizeof(oldFormID) ||
                        a_interface->ReadRecordData(useCount) != sizeof(useCount)) {
                        break;
                    }
                    if (auto resolved = a_interface->ResolveFormID(oldFormID); resolved) {
                        g_useCounts[*resolved] = useCount;
                    }
                }
                continue;
            }
            if (type == FAVORITES_V2_RECORD && (version == 1 || version == FAVORITES_V2_RECORD_VERSION)) {
                std::uint32_t count = 0;
                if (a_interface->ReadRecordData(count) != sizeof(count)) {
                    continue;
                }
                g_initialized = true;
                Log("loaded favorites record version=" + std::to_string(version) +
                    " count=" + std::to_string(count));
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::uint32_t oldFormID = 0;
                    StoredFavorite favorite;
                    if (a_interface->ReadRecordData(oldFormID) != sizeof(oldFormID) ||
                        (version >= 2 && a_interface->ReadRecordData(favorite.instanceKey) != sizeof(favorite.instanceKey)) ||
                        a_interface->ReadRecordData(favorite.hotkeySlot) != sizeof(favorite.hotkeySlot) ||
                        a_interface->ReadRecordData(favorite.addOrder) != sizeof(favorite.addOrder)) {
                        break;
                    }
                    if (auto resolved = a_interface->ResolveFormID(oldFormID); resolved) {
                        favorite.formID = *resolved;
                        if (favorite.hotkeySlot >= 12) {
                            favorite.hotkeySlot = QUICKKEY_NONE;
                        }
                        g_favorites.push_back(favorite);
                    }
                }
                continue;
            }
            if (length) {
                std::vector<std::byte> ignored(length);
                a_interface->ReadRecordData(ignored.data(), length);
            }
        }
        RebuildIndex();
        Log("load callback complete favorites=" + std::to_string(g_favorites.size()));
    }

    void Revert(const F4SE::SerializationInterface*)
    {
        std::lock_guard lock(g_mutex);
        g_useCounts.clear();
        g_favorites.clear();
        g_index.clear();
        g_nextAddOrder = 1;
        g_initialized = false;
        g_legacyMarkerMigrationAttempted = false;
    }
}

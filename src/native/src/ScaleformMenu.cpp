#include "Aozora/ScaleformMenu.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>

#include "F4SE/F4SE.h"
#include "Scaleform/G/GFx_ASMovieRootBase.h"
#include "RE/B/BSScaleformManager.h"
#include "RE/B/BSTimer.h"
#include "RE/B/BS_BUTTON_CODE.h"
#include "RE/B/ButtonEvent.h"
#include "RE/F/FavoritesManager.h"
#include "RE/Fallout.h"
#include "RE/M/Main.h"
#include "RE/M/MenuControls.h"
#include "RE/M/MenuOpenCloseEvent.h"
#include "RE/I/INPUT_EVENT_TYPE.h"
#include "RE/P/PlayerControls.h"
#include "RE/P/PlayerInputHandler.h"
#include "RE/P/PipboyDataManager.h"
#include "RE/P/PipboyPrimitiveValue.h"
#include "RE/P/PipboyMenu.h"
#include "RE/T/ThumbstickEvent.h"
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"
#include "RE/U/UI_MENU_FLAGS.h"
#include "RE/U/UI_MESSAGE_TYPE.h"
#include "Aozora/Settings.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Aozora::SWF
{
    namespace
    {
        using FunctionParams = Scaleform::GFx::FunctionHandler::Params;
        using PipboyButtonEvent_t = void (*)(RE::BSInputEventUser*, const RE::ButtonEvent*);
        PipboyButtonEvent_t g_originalPipboyButtonEvent{ nullptr };
        bool g_pipboyHookInstalled{ false };
        std::atomic_bool g_pipboyFavoriteRetryPending{ false };
        FavoritesMenu* g_menu{ nullptr };
        std::uint64_t g_menuSessionCounter{ 0 };
        std::uint32_t g_lastCategoryIndex{ 0 };
        bool g_registered{ false };
        bool g_favoritesHookInstalled{ false };
        std::atomic_bool g_inputRegistrationStarted{ false };
        std::atomic_bool g_menuInputRegistered{ false };
        std::atomic_bool g_playerInputRegistered{ false };
        std::atomic_bool g_closeQueued{ false };
        std::atomic_bool g_customOpenRequested{ false };
        std::atomic_bool g_pipboyQuickkeyHintShown{ false };
        bool g_vanillaMenuSinkRegistered{ false };
        constexpr std::array<std::string_view, 5> MASCOT_SERIES_NAMES{
            "Chat", "Snack", "Mechanic", "Explorer", "Groom" };
        using FavoritesShouldHandleEvent_t = bool (*)(RE::BSInputEventUser*, const RE::InputEvent*);
        FavoritesShouldHandleEvent_t g_originalFavoritesShouldHandleEvent{ nullptr };
        RE::PlayerInputHandler* g_playerInputHandler{ nullptr };

        class DirectNativeFunctionHandler final : public Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                if (g_menu) {
                    g_menu->Call(a_params);
                }
            }
        };

        std::uint64_t ParamUInt64(const FunctionParams& a_params, std::uint32_t a_index)
        {
            if (!a_params.args || a_index >= a_params.argCount) {
                return 0;
            }
            const auto& value = a_params.args[a_index];
            if (value.IsString()) {
                const char* text = value.GetString();
                if (!text || !text[0]) {
                    return 0;
                }
                std::uint64_t parsed = 0;
                const auto* first = text;
                const auto* last = text + std::char_traits<char>::length(text);
                if (last - first > 2 && first[0] == '0' && (first[1] == 'x' || first[1] == 'X')) {
                    first += 2;
                }
                std::from_chars(first, last, parsed, 16);
                return parsed;
            }
            if (value.IsUInt()) {
                return value.GetUInt();
            }
            if (value.IsInt()) {
                return value.GetInt() >= 0 ? static_cast<std::uint64_t>(value.GetInt()) : 0;
            }
            if (value.IsNumber()) {
                return value.GetNumber() >= 0.0 ? static_cast<std::uint64_t>(value.GetNumber()) : 0;
            }
            return 0;
        }

        std::uint32_t ParamUInt32(const FunctionParams& a_params, std::uint32_t a_index)
        {
            return static_cast<std::uint32_t>(ParamUInt64(a_params, a_index));
        }

        std::string ParamString(const FunctionParams& a_params, std::uint32_t a_index)
        {
            if (!a_params.args || a_index >= a_params.argCount || !a_params.args[a_index].IsString() ||
                !a_params.args[a_index].GetString()) {
                return {};
            }
            return a_params.args[a_index].GetString();
        }

        void Log(std::string_view a_message);
        std::uint32_t HotkeySlotForCode(RE::BS_BUTTON_CODE a_code);
        bool IsDPadDirection(const RE::InputEvent* a_event);

        bool IsPipboyOpen()
        {
            auto* ui = RE::UI::GetSingleton();
            return ui && ui->GetMenuOpen(RE::BSFixedString("PipboyMenu"));
        }

        bool IsPipboyInventoryContext()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->GetMenuOpen<RE::PipboyMenu>()) {
                return false;
            }
            auto pipboy = ui->GetMenu<RE::PipboyMenu>();
            return pipboy && pipboy->inventoryMenuObj.menuObj &&
                pipboy->inventoryMenuObj.menuObj->IsObject();
        }

        bool GfxValueToUInt(const Scaleform::GFx::Value& a_value, std::uint32_t& a_result)
        {
            if (a_value.IsUInt()) {
                a_result = a_value.GetUInt();
                return true;
            }
            if (a_value.IsInt() && a_value.GetInt() >= 0) {
                a_result = static_cast<std::uint32_t>(a_value.GetInt());
                return true;
            }
            if (a_value.IsNumber() && a_value.GetNumber() >= 0.0) {
                a_result = static_cast<std::uint32_t>(a_value.GetNumber());
                return true;
            }
            if (a_value.IsString() && a_value.GetString()) {
                const std::string text(a_value.GetString());
                try {
                    a_result = static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
                    return true;
                } catch (...) {
                    try {
                        a_result = static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
                        return true;
                    } catch (...) {
                        return false;
                    }
                }
            }
            return false;
        }

        std::uint32_t FormIDFromGfxEntry(const Scaleform::GFx::Value& a_entry, int a_depth = 0)
        {
            if (!a_entry.IsObject() || a_depth > 3) {
                return 0;
            }
            for (const auto* memberName : { "formID", "FormID", "formId" }) {
                Scaleform::GFx::Value member;
                std::uint32_t formID = 0;
                if (a_entry.GetMember(memberName, &member) &&
                    GfxValueToUInt(member, formID) && RE::TESForm::GetFormByID(formID)) {
                    return formID;
                }
            }
            for (const auto* memberName : {
                     "data", "item", "entry", "invItem", "entryObject", "object", "itemObj",
                     "itemData", "dataObj", "baseForm", "baseObject", "form", "value", "_value",
                     "linkedObject" }) {
                Scaleform::GFx::Value nested;
                if (a_entry.GetMember(memberName, &nested)) {
                    if (const auto formID = FormIDFromGfxEntry(nested, a_depth + 1); formID != 0) {
                        return formID;
                    }
                }
            }
            return 0;
        }

        bool StackIDFromGfxEntry(
            const Scaleform::GFx::Value& a_entry,
            std::uint32_t& a_stackID,
            int a_depth = 0)
        {
            if (!a_entry.IsObject() || a_depth > 3) {
                return false;
            }
            // These are stack indexes used by Pip-Boy list entries. Do not
            // treat the row's general index as a stack ID: rows from other
            // forms would otherwise select a different weapon instance.
            for (const auto* memberName : { "stackID", "stackId", "stackIndex", "_stackID", "_stackIndex" }) {
                Scaleform::GFx::Value member;
                if (a_entry.GetMember(memberName, &member) && GfxValueToUInt(member, a_stackID)) {
                    return true;
                }
            }
            for (const auto* memberName : {
                     "data", "item", "entry", "invItem", "entryObject", "object", "itemObj",
                     "itemData", "dataObj", "stackData" }) {
                Scaleform::GFx::Value nested;
                if (a_entry.GetMember(memberName, &nested) &&
                    StackIDFromGfxEntry(nested, a_stackID, a_depth + 1)) {
                    return true;
                }
            }
            return false;
        }

        bool SelectedIndexFromGfxObject(
            const Scaleform::GFx::Value& a_object,
            std::uint32_t& a_selectedIndex)
        {
            if (!a_object.IsObject()) {
                return false;
            }
            for (const auto* memberName : { "selectedIndex", "iSelectedIndex", "_selectedIndex" }) {
                Scaleform::GFx::Value member;
                if (a_object.GetMember(memberName, &member) &&
                    GfxValueToUInt(member, a_selectedIndex)) {
                    return true;
                }
            }
            return false;
        }

        bool SelectedIndexFromGfxContainer(
            const Scaleform::GFx::Value& a_object,
            std::uint32_t& a_selectedIndex,
            std::uint32_t a_depth = 0)
        {
            if (!a_object.IsObject() || a_depth > 3) {
                return false;
            }
            if (SelectedIndexFromGfxObject(a_object, a_selectedIndex)) {
                return true;
            }
            for (const auto* memberName : {
                     "CurrentPage", "List_mc", "ItemList_mc", "itemList", "itemlist",
                     "InventoryList_mc", "InventoryList", "InvList", "scrollList", "List" }) {
                Scaleform::GFx::Value nested;
                if (a_object.GetMember(memberName, &nested) && nested.IsObject() &&
                    SelectedIndexFromGfxContainer(nested, a_selectedIndex, a_depth + 1)) {
                    return true;
                }
            }
            return false;
        }

        bool SelectedPipboyInventorySelection(
            std::uint32_t& a_formID,
            std::uint32_t& a_stackID)
        {
            a_formID = 0;
            a_stackID = 0xFFFFFFFFu;
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->GetMenuOpen<RE::PipboyMenu>()) {
                Log("PIPBOY_SELECTION failed=menu-not-open");
                return false;
            }
            auto pipboy = ui->GetMenu<RE::PipboyMenu>();
            if (!pipboy || !pipboy->inventoryMenuObj.menuObj ||
                !pipboy->inventoryMenuObj.menuObj->IsObject()) {
                Log("PIPBOY_SELECTION failed=inventory-scaleform-object");
                return false;
            }

            const Scaleform::GFx::Value* rootList = pipboy->inventoryMenuObj.menuObj;
            const Scaleform::GFx::Value* listObject = pipboy->inventoryMenuObj.menuObj;
            Scaleform::GFx::Value nestedList;
            for (const auto* memberName : {
                     "List_mc", "ItemList_mc", "itemList", "itemlist", "InventoryList_mc", "InventoryList" }) {
                if (listObject->GetMember(memberName, &nestedList) && nestedList.IsObject()) {
                    listObject = std::addressof(nestedList);
                    break;
                }
            }

            Scaleform::GFx::Value selectedEntry;
            if (listObject->GetMember("selectedEntry", &selectedEntry)) {
                a_formID = FormIDFromGfxEntry(selectedEntry);
                StackIDFromGfxEntry(selectedEntry, a_stackID);
            }

            std::uint32_t selectedIndex = 0;
            bool hasSelectedIndex = SelectedIndexFromGfxObject(*listObject, selectedIndex);
            if (!hasSelectedIndex && listObject != rootList) {
                hasSelectedIndex = SelectedIndexFromGfxObject(*rootList, selectedIndex);
            }
            if (a_formID == 0 && hasSelectedIndex) {
                const std::array<Scaleform::GFx::Value*, 2> dataObjects{
                    pipboy->inventoryMenuObj.dataObj, std::addressof(pipboy->dataObj) };
                for (const auto* dataObject : dataObjects) {
                    if (!dataObject || !dataObject->IsObject()) {
                        continue;
                    }
                    for (const auto* memberName : { "_InvSelectedItems", "InvSelectedItems" }) {
                        Scaleform::GFx::Value items;
                        if (!dataObject->GetMember(memberName, &items) || !items.IsArray() ||
                            selectedIndex >= items.GetArraySize()) {
                            continue;
                        }
                        if (items.GetElement(selectedIndex, &selectedEntry)) {
                            a_formID = FormIDFromGfxEntry(selectedEntry);
                            StackIDFromGfxEntry(selectedEntry, a_stackID);
                            break;
                        }
                    }
                    if (a_formID != 0) {
                        break;
                    }
                }
            }
            if (a_formID == 0) {
                Log("PIPBOY_SELECTION failed=form-not-found");
                return false;
            }
            Log("PIPBOY_SELECTION form=" + std::to_string(a_formID) +
                " stack=" + (a_stackID == 0xFFFFFFFFu ? std::string("unknown") :
                    std::to_string(a_stackID)));
            return true;
        }

        void QueuePipboyInventoryRefresh()
        {
            auto refresh = [] {
                if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                    queue->AddMessage(RE::BSFixedString("PipboyMenu"),
                        RE::UI_MESSAGE_TYPE::kInventoryUpdate);
                    Log("PIPBOY_REFRESH inventory-update");
                }
            };
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddUITask(std::move(refresh));
            } else {
                refresh();
            }
        }

        bool PipboySelectedIndexFromRoot(
            const Scaleform::GFx::Value* a_root,
            std::uint32_t& a_selectedIndex)
        {
            if (!a_root || !a_root->IsObject()) {
                return false;
            }
            Scaleform::GFx::Value page;
            if (a_root->GetMember("CurrentPage", &page) && page.IsObject()) {
                Scaleform::GFx::Value list;
                if (page.GetMember("List_mc", &list) && list.IsObject() &&
                    SelectedIndexFromGfxObject(list, a_selectedIndex)) {
                    return true;
                }
            }
            // Some Pip-Boy variants expose the inventory page only through
            // the native submenu object. Keep this fallback for FallUI/FIS
            // builds that retain the vanilla menu root but change getters.
            auto* ui = RE::UI::GetSingleton();
            auto pipboy = ui ? ui->GetMenu<RE::PipboyMenu>() : nullptr;
            if (pipboy && pipboy->inventoryMenuObj.menuObj &&
                pipboy->inventoryMenuObj.menuObj->IsObject()) {
                Scaleform::GFx::Value list;
                if (pipboy->inventoryMenuObj.menuObj->GetMember("List_mc", &list) &&
                    list.IsObject() && SelectedIndexFromGfxObject(list, a_selectedIndex)) {
                    return true;
                }
            }
            return false;
        }

        bool PipboyInventoryPageFromRoot(const Scaleform::GFx::Value* a_root)
        {
            if (!a_root || !a_root->IsObject()) {
                return false;
            }
            Scaleform::GFx::Value page;
            if (a_root->GetMember("CurrentPage", &page) && page.IsObject()) {
                Scaleform::GFx::Value list;
                return page.GetMember("List_mc", &list) && list.IsObject();
            }
            auto* ui = RE::UI::GetSingleton();
            auto pipboy = ui ? ui->GetMenu<RE::PipboyMenu>() : nullptr;
            return pipboy && pipboy->inventoryMenuObj.menuObj &&
                pipboy->inventoryMenuObj.menuObj->IsObject();
        }

        bool SelectedPipboyInventoryIndex(std::uint32_t& a_selectedIndex)
        {
            a_selectedIndex = 0;
            auto* ui = RE::UI::GetSingleton();
            auto pipboy = ui ? ui->GetMenu<RE::PipboyMenu>() : nullptr;
            if (!pipboy || !pipboy->inventoryMenuObj.menuObj ||
                !pipboy->inventoryMenuObj.menuObj->IsObject()) {
                return false;
            }

            if (pipboy->menuObj.IsObject() &&
                SelectedIndexFromGfxContainer(pipboy->menuObj, a_selectedIndex)) {
                Log("PIPBOY_SELECTION index-source=pipboy-menu");
                return true;
            }
            if (SelectedIndexFromGfxContainer(*pipboy->inventoryMenuObj.menuObj, a_selectedIndex)) {
                Log("PIPBOY_SELECTION index-source=inventory-menu");
                return true;
            }
            return false;
        }

        bool ReadPipboyUIntMember(
            RE::PipboyObject* a_object,
            std::uint32_t& a_value,
            std::initializer_list<const char*> a_names)
        {
            if (!a_object) {
                return false;
            }
            for (const auto* name : a_names) {
                if (!name) {
                    continue;
                }
                auto* value = a_object->GetMember<RE::PipboyPrimitiveValue<std::uint32_t>*>(
                    RE::BSFixedString(name));
                if (value) {
                    a_value = value->m_value;
                    return true;
                }
            }
            return false;
        }

        bool NativePipboySelectionByIndex(
            std::uint32_t a_selectedIndex,
            std::uint32_t& a_formID,
            std::uint32_t& a_stackID)
        {
            a_formID = 0;
            a_stackID = 0xFFFFFFFFu;
            auto* dataManager = RE::PipboyDataManager::GetSingleton();
            if (!dataManager) {
                Log("PIPBOY_SELECTION_NATIVE failed=data-manager");
                return false;
            }

            auto& inventory = dataManager->inventoryData;
            if (a_selectedIndex >= inventory.sortedItems.size()) {
                Log("PIPBOY_SELECTION_NATIVE failed=index-out-of-range index=" +
                    std::to_string(a_selectedIndex) +
                    " count=" + std::to_string(inventory.sortedItems.size()));
                return false;
            }
            auto* selected = inventory.sortedItems[a_selectedIndex];
            if (!selected) {
                Log("PIPBOY_SELECTION_NATIVE failed=selected-object-null index=" +
                    std::to_string(a_selectedIndex));
                return false;
            }

            std::uint32_t candidate = 0;
            if (!ReadPipboyUIntMember(selected, candidate,
                    { "formID", "formId", "itemID", "itemId", "itemIndex", "id" }) ||
                !RE::TESForm::GetFormByID(candidate)) {
                Log("PIPBOY_SELECTION_NATIVE failed=form-not-found index=" +
                    std::to_string(a_selectedIndex));
                return false;
            }
            a_formID = candidate;

            if (!ReadPipboyUIntMember(selected, a_stackID,
                    { "stackID", "stackId", "stackIndex", "itemStackIndex" })) {
                // A row without a stack field is safe only when the native
                // inventory has a single stack for this form. Never guess a
                // stack for split or differently-modded items.
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player || !player->inventoryList) {
                    Log("PIPBOY_SELECTION_NATIVE failed=inventory-list form=" +
                        std::to_string(candidate));
                    return false;
                }
                std::uint32_t stackCount = 0;
                for (auto& item : player->inventoryList->data) {
                    if (!item.object || item.object->GetFormID() != candidate) {
                        continue;
                    }
                    for (auto* stack = item.stackData.get(); stack;
                        stack = stack->nextStack.get()) {
                        ++stackCount;
                    }
                    break;
                }
                if (stackCount != 1) {
                    Log("PIPBOY_SELECTION_NATIVE failed=stack-not-exact form=" +
                        std::to_string(candidate) +
                        " stacks=" + std::to_string(stackCount));
                    return false;
                }
                a_stackID = 0;
            }

            Log("PIPBOY_SELECTION_NATIVE index=" + std::to_string(a_selectedIndex) +
                " form=" + std::to_string(a_formID) +
                " stack=" + std::to_string(a_stackID));
            return true;
        }

        bool IsFavoriteUserEvent(std::string_view a_event)
        {
            return a_event == "Favorite" || a_event == "Favorites" ||
                a_event == "RShoulder";
        }

        bool IsQuickkeyUserEvent(std::string_view a_event)
        {
            if (a_event == "Quickkey0" || a_event == "QuickkeyMinus" ||
                a_event == "Quickkey-" || a_event == "QuickkeyEquals" ||
                a_event == "Quickkey=") {
                return true;
            }
            constexpr std::string_view prefix = "Quickkey";
            const auto suffix = a_event.starts_with(prefix) ?
                a_event.substr(prefix.size()) : std::string_view{};
            return suffix.size() == 1 && suffix[0] >= '1' && suffix[0] <= '9';
        }

        bool IsQuickkeyButtonEvent(const RE::InputEvent* a_event)
        {
            if (!a_event || a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                return false;
            }
            const auto* button = a_event->As<RE::ButtonEvent>();
            return button && IsQuickkeyUserEvent(button->QUserEvent().c_str());
        }

        bool ShowPipboyQuickkeyDisabledHint()
        {
            if (g_pipboyQuickkeyHintShown.exchange(true)) {
                return false;
            }
            // Pip-Boy quickkeys are deliberately disabled. Hotkeys are edited
            // in Aozora Favorites, while gameplay activation remains separate.
            RE::SendHUDMessage::ShowHUDMessage("$AOZORA_HOTKEY_NOTICE", nullptr, true, false);
            Log("PIPBOY_NATIVE_HINT numeric-disabled shown=1");
            return true;
        }

        void HardBlockPipboyQuickkey(RE::ButtonEvent* a_event,
            std::string_view a_layer)
        {
            if (!a_event) {
                return;
            }
            const auto originalEvent = std::string(a_event->QUserEvent().c_str());
            const bool released = a_event->QReleased();
            // IMenu::OnButtonEvent forwards its event to Scaleform without
            // consulting handled, so kStop alone is not sufficient here.
            // Remove the logical and physical Quickkey identity as well.
            a_event->strUserEvent = RE::BSFixedString("AozoraBlockedQuickkey");
            a_event->idCode = -1;
            a_event->disabled = true;
            a_event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
            if (released) {
                const bool hintShown = ShowPipboyQuickkeyDisabledHint();
                Log("PIPBOY_QUICKKEY_BLOCK layer=" + std::string(a_layer) +
                    " event=" + originalEvent +
                    " hardBlocked=1 hint=" +
                    (hintShown ? "shown" : "suppressed") +
                    " consumed=1 originalCalled=0");
            }
        }

        void HardBlockFavoritesEscape(RE::ButtonEvent* a_event,
            std::string_view a_layer)
        {
            if (!a_event) {
                return;
            }
            const bool released = a_event->QReleased();
            a_event->strUserEvent = RE::BSFixedString("AozoraBlockedMenuEscape");
            a_event->idCode = -1;
            a_event->disabled = true;
            a_event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
            if (released) {
                Log("MENU_ESCAPE_BLOCK layer=" + std::string(a_layer) +
                    " consumed=1 originalCalled=0");
            }
        }

        void HardBlockGameplayDPad(RE::ButtonEvent* a_event)
        {
            if (!a_event) {
                return;
            }
            a_event->strUserEvent = RE::BSFixedString("AozoraBlockedGameplayDPad");
            a_event->idCode = -1;
            a_event->disabled = true;
            a_event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        }

        void SchedulePipboyFavoriteRetry(std::uint32_t a_attempt);

        void RunPipboyFavoriteRetry(std::uint32_t a_attempt)
        {
            g_pipboyFavoriteRetryPending = false;
            if (!IsPipboyOpen() || !IsPipboyInventoryContext()) {
                Log("PIPBOY_NATIVE_INPUT selection-retry failed");
                return;
            }
            std::uint32_t selectedIndex = 0;
            const bool handled = SelectedPipboyInventoryIndex(selectedIndex) &&
                ToggleFavoriteForPipboyIndex(selectedIndex);
            if (handled) {
                Log("PIPBOY_NATIVE_INPUT selection-retry success");
                return;
            }
            if (a_attempt < 2) {
                SchedulePipboyFavoriteRetry(a_attempt + 1);
                return;
            }
            Log("PIPBOY_NATIVE_INPUT selection-retry failed");
        }

        void SchedulePipboyFavoriteRetry(std::uint32_t a_attempt)
        {
            if (g_pipboyFavoriteRetryPending.exchange(true)) {
                return;
            }
            Log("PIPBOY_NATIVE_INPUT selection-retry queued");
            auto retry = [a_attempt] { RunPipboyFavoriteRetry(a_attempt); };
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddUITask(std::move(retry));
            } else {
                retry();
            }
        }

        void PipboyButtonEventHook(
            RE::BSInputEventUser* a_inputUser,
            const RE::ButtonEvent* a_event)
        {
            auto forward = [&] {
                if (g_originalPipboyButtonEvent) {
                    g_originalPipboyButtonEvent(a_inputUser, a_event);
                }
            };
            if (!a_event || !a_inputUser) {
                forward();
                return;
            }
            auto* menu = static_cast<RE::PipboyMenu*>(
                static_cast<RE::IMenu*>(a_inputUser));
            auto* ui = RE::UI::GetSingleton();
            auto current = ui ? ui->GetMenu<RE::PipboyMenu>() : nullptr;
            if (!menu || !current || current.get() != menu || !IsPipboyOpen()) {
                forward();
                return;
            }

            const auto userEvent = std::string_view(a_event->QUserEvent().c_str());
            const bool favoriteEvent = IsFavoriteUserEvent(userEvent);
            const bool quickkeyEvent = IsQuickkeyUserEvent(userEvent);
            if (quickkeyEvent) {
                HardBlockPipboyQuickkey(const_cast<RE::ButtonEvent*>(a_event),
                    "pipboy-native");
                return;
            }
            if (!a_event->QReleased() || !favoriteEvent) {
                forward();
                return;
            }

            Log("PIPBOY_NATIVE_INPUT event=" + std::string(userEvent));
            if (!IsPipboyInventoryContext()) {
                Log("PIPBOY_NATIVE_INPUT consumed=0 originalCalled=1 reason=context");
                forward();
                return;
            }
            std::uint32_t selectedIndex = 0;
            if (!SelectedPipboyInventoryIndex(selectedIndex)) {
                Log("PIPBOY_NATIVE_INPUT consumed=1 originalCalled=0 reason=selection");
                SchedulePipboyFavoriteRetry(1);
                return;
            }
            const bool handled = ToggleFavoriteForPipboyIndex(selectedIndex);
            if (handled) {
                Log("PIPBOY_NATIVE_INPUT action=favorite-toggle source=favorite "
                    "consumed=1 originalCalled=0");
                return;
            }
            Log("PIPBOY_NATIVE_INPUT consumed=1 originalCalled=0 reason=selection");
            SchedulePipboyFavoriteRetry(1);
        }

        void InstallPipboyNativeInputHook()
        {
            if (g_pipboyHookInstalled) {
                return;
            }
            Log("PIPBOY_NATIVE_HOOK install begin");
            try {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PipboyMenu[1] };
                g_originalPipboyButtonEvent = reinterpret_cast<PipboyButtonEvent_t>(
                    vtable.write_vfunc(8, PipboyButtonEventHook));
                if (!g_originalPipboyButtonEvent) {
                    Log("PIPBOY_NATIVE_HOOK install failed reason=original-null");
                    return;
                }
                g_pipboyHookInstalled = true;
                Log("PIPBOY_NATIVE_HOOK install success runtime=address-library-vtable slot=8");
            } catch (...) {
                g_originalPipboyButtonEvent = nullptr;
                Log("PIPBOY_NATIVE_HOOK install failed reason=exception");
            }
        }

        bool IsFavoritesTrigger(const RE::InputEvent* a_event)
        {
            if (!a_event || a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                return false;
            }
            const auto* button = a_event->As<RE::ButtonEvent>();
            if (!button) {
                return false;
            }
            const auto userEvent = std::string_view(button->QUserEvent().c_str());
            return (button->device == RE::INPUT_DEVICE::kKeyboard &&
                    button->GetBSButtonCode() == RE::BS_BUTTON_CODE::kF) ||
                (button->device != RE::INPUT_DEVICE::kKeyboard &&
                    (userEvent == "Favorites" || userEvent == "Quickkeys" || userEvent == "Quickkey"));
        }

        bool IsMenuCloseTrigger(const RE::InputEvent* a_event)
        {
            if (!a_event || a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                return false;
            }
            const auto* button = a_event->As<RE::ButtonEvent>();
            if (!button) {
                return false;
            }
            const auto code = button->GetBSButtonCode();
            const auto userEvent = std::string_view(button->QUserEvent().c_str());
            return code == RE::BS_BUTTON_CODE::kEscape ||
                code == RE::BS_BUTTON_CODE::kTab ||
                code == RE::BS_BUTTON_CODE::kBackspace ||
                code == RE::BS_BUTTON_CODE::kBButton ||
                code == RE::BS_BUTTON_CODE::kBack ||
                ((button->device == RE::INPUT_DEVICE::kGamepad) &&
                    (userEvent == "Cancel" || userEvent == "Back"));
        }

        bool IsConsoleTrigger(const RE::InputEvent* a_event)
        {
            if (!a_event || a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                return false;
            }
            const auto* button = a_event->As<RE::ButtonEvent>();
            if (!button) {
                return false;
            }
            const auto userEvent = std::string_view(button->QUserEvent().c_str());
            return userEvent == "Console" || userEvent == "ConsoleOpen";
        }

        std::uint32_t HotkeySlotForCode(RE::BS_BUTTON_CODE a_code)
        {
            const auto value = static_cast<std::int32_t>(a_code);
            if (value >= static_cast<std::int32_t>(RE::BS_BUTTON_CODE::k1) &&
                value <= static_cast<std::int32_t>(RE::BS_BUTTON_CODE::k9)) {
                return static_cast<std::uint32_t>(value - static_cast<std::int32_t>(RE::BS_BUTTON_CODE::k1));
            }
            if (a_code == RE::BS_BUTTON_CODE::k0) {
                return 9;
            }
            if (a_code == RE::BS_BUTTON_CODE::kMinus) {
                return 10;
            }
            if (a_code == RE::BS_BUTTON_CODE::kEquals) {
                return 11;
            }
            return 0xFFFFFFFFu;
        }

        bool IsDPadDirection(const RE::InputEvent* a_event)
        {
            if (!a_event || a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                return false;
            }
            const auto* button = a_event->As<RE::ButtonEvent>();
            if (!button) {
                return false;
            }
            const auto userEvent = std::string_view(button->QUserEvent().c_str());
            return userEvent == "QuickkeyUp" || userEvent == "QuickkeyDown" ||
                userEvent == "QuickkeyLeft" || userEvent == "QuickkeyRight" ||
                button->GetBSButtonCode() == RE::BS_BUTTON_CODE::kDPAD_Up ||
                button->GetBSButtonCode() == RE::BS_BUTTON_CODE::kDPAD_Down ||
                button->GetBSButtonCode() == RE::BS_BUTTON_CODE::kDPAD_Left ||
                button->GetBSButtonCode() == RE::BS_BUTTON_CODE::kDPAD_Right;
        }

        bool OtherMenuOpenExceptVanillaFavorites()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return true;
            }
            for (const char* name : {
                "BarberMenu", "BookMenu", "BarterMenu", "CharGenMenu", "Console",
                "ContainerMenu", "CookingMenu", "CraftingMenu", "DialogueMenu",
                "ExamineMenu", "LoadingMenu", "LockpickingMenu", "LooksMenu",
                "MainMenu", "MessageBoxMenu", "NameMenu", "PauseMenu", "PipboyMenu",
                "PowerArmorModMenu", "RobotModMenu", "SleepWaitMenu", "SPECIALMenu",
                "TerminalMenu", "VATSMenu", "WorkshopMenu", "QuickContainerMenu",
                "AozoraFavoritesMenu" }) {
                if (ui->GetMenuOpen(RE::BSFixedString(name))) {
                    return true;
                }
            }
            return false;
        }

        bool IsGameplayFavoritesContext()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->menuMode != 0) {
                return false;
            }
            if (auto* main = RE::Main::GetSingleton(); main && main->inMenuMode) {
                return false;
            }
            if (auto* controls = RE::PlayerControls::GetSingleton();
                controls && controls->blockPlayerInput) {
                return false;
            }
            return !IsPipboyOpen() && !OtherMenuOpenExceptVanillaFavorites();
        }

        bool IsVanillaFavoritesOpen()
        {
            auto* ui = RE::UI::GetSingleton();
            return ui && ui->GetMenuOpen(RE::BSFixedString("FavoritesMenu"));
        }

        void CloseVanillaFavoritesMenu()
        {
            if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                queue->AddMessage(RE::BSFixedString("FavoritesMenu"), RE::UI_MESSAGE_TYPE::kHide);
                Log("VANILLA_MENU_CLOSE queue-hide");
            }
        }

        void QueueVanillaFavoritesRefresh()
        {
            auto refresh = [] {
                if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                    queue->AddMessage(RE::BSFixedString("FavoritesMenu"),
                        RE::UI_MESSAGE_TYPE::kInventoryUpdate);
                    Log("VANILLA_MENU_REFRESH inventory-update");
                }
            };
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddUITask(std::move(refresh));
            } else {
                refresh();
            }
        }

        class VanillaFavoritesMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent& a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (a_event.menuName == RE::BSFixedString("PipboyMenu")) {
                    g_pipboyQuickkeyHintShown = false;
                    if (a_event.opening) {
                        // Remove numeric Favorite markers left by older
                        // versions or by the vanilla manager before the
                        // Pip-Boy inventory is displayed. Aozora's own
                        // marker (-1) is restored by ClearVanillaFavorites.
                        ClearVanillaFavorites();
                    }
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (a_event.menuName != RE::BSFixedString("FavoritesMenu")) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                Log(a_event.opening ?
                    "VANILLA_MENU_EVENT opening=true" :
                    "VANILLA_MENU_EVENT opening=false");
                if (a_event.opening) {
                    ClearVanillaFavorites();
                    QueueVanillaFavoritesRefresh();
                    if (g_customOpenRequested.load() || FavoritesMenu::IsOpen()) {
                        CloseVanillaFavoritesMenu();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        VanillaFavoritesMenuSink g_vanillaMenuSink;

        bool FavoritesManagerShouldHandleEventHook(
            RE::BSInputEventUser* a_self,
            const RE::InputEvent* a_event)
        {
            if (IsDPadDirection(a_event) && IsGameplayFavoritesContext()) {
                auto* button = const_cast<RE::ButtonEvent*>(a_event->As<RE::ButtonEvent>());
                const bool justPressed = button && button->QJustPressed();
                HardBlockGameplayDPad(button);
                if (justPressed) {
                    Log("FAVORITES_MANAGER_INPUT dpad blocked=1 originalCalled=0");
                }
                return false;
            }
            if (IsPipboyOpen() && IsQuickkeyButtonEvent(a_event)) {
                const auto* button = a_event->As<RE::ButtonEvent>();
                const auto eventName = button ?
                    std::string(button->QUserEvent().c_str()) : std::string{};
                HardBlockPipboyQuickkey(const_cast<RE::ButtonEvent*>(button),
                    "favorites-manager");
                if (button && button->QReleased()) {
                    Log("FAVORITES_MANAGER_INPUT event=" +
                        eventName +
                        " pipboyOpen=1 blocked=1 originalCalled=0");
                }
                return false;
            }
            // Pip-Boy owns its own favorite input path. Preserve that
            // path while replacing the gameplay Favorites action.
            if (IsFavoritesTrigger(a_event) &&
                (IsGameplayFavoritesContext() || IsVanillaFavoritesOpen())) {
                const auto* button = a_event->As<RE::ButtonEvent>();
                if (button && button->QJustPressed()) {
                    Log("VANILLA_MANAGER_BLOCKED input=F/Favorites");
                }
                return false;
            }
            const auto* button = a_event ? a_event->As<RE::ButtonEvent>() : nullptr;
            if (button && button->QJustPressed() && IsFavoritesTrigger(a_event)) {
                Log("VANILLA_MANAGER_PASSTHROUGH reason=blocked-context");
            }
            return g_originalFavoritesShouldHandleEvent ?
                g_originalFavoritesShouldHandleEvent(a_self, a_event) : false;
        }

        class OpenInputHandler final : public RE::BSInputEventUser
        {
        public:
            bool ShouldHandleEvent(const RE::InputEvent* a_event) override
            {
                // Match the stable community pattern: receive every button
                // and thumbstick event, then filter inside the callbacks.
                // Filtering here can miss Quickkeys when the game has not yet
                // populated QUserEvent for the current input device.
                return a_event && (a_event->eventType == RE::INPUT_EVENT_TYPE::kButton ||
                    a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick);
            }

            void OnButtonEvent(const RE::ButtonEvent* a_event) override
            {
                if (a_event && FavoritesMenu::IsOpen() &&
                    a_event->GetBSButtonCode() == RE::BS_BUTTON_CODE::kEscape) {
                    auto* event = const_cast<RE::ButtonEvent*>(a_event);
                    const bool justPressed = a_event->QJustPressed();
                    HardBlockFavoritesEscape(event, "menu-controls");
                    if (justPressed) {
                        Log("MENU_INPUT close-key action=queue-hide");
                        FavoritesMenu::Close();
                    }
                    return;
                }
                if (a_event && IsPipboyOpen() &&
                    IsQuickkeyUserEvent(a_event->QUserEvent().c_str())) {
                    // This handler is registered at the front of MenuControls.
                    // Rewrite the event before it can reach Pip-Boy or any
                    // vanilla Quickkey consumer.
                    HardBlockPipboyQuickkey(const_cast<RE::ButtonEvent*>(a_event),
                        "menu-controls");
                    return;
                }
                if (a_event && IsDPadDirection(a_event) &&
                    IsGameplayFavoritesContext()) {
                    const auto action = DPadActionForInput(
                        a_event->QUserEvent().c_str(), a_event->GetBSButtonCode());
                    auto* event = const_cast<RE::ButtonEvent*>(a_event);
                    const bool justPressed = a_event->QJustPressed();
                    HardBlockGameplayDPad(event);
                    if (justPressed) {
                        if (action == 2) {
                            Log("MENU_CONTROLS_CAPTURED input=DPad action=open-custom-menu consumed=1");
                            FavoritesMenu::Open();
                        } else {
                            Log("MENU_CONTROLS_CAPTURED input=DPad action=no-action consumed=1");
                        }
                    }
                    return;
                }
                if (a_event && a_event->QJustPressed() &&
                    a_event->device == RE::INPUT_DEVICE::kKeyboard &&
                    IsGameplayFavoritesContext()) {
                    const auto slot = HotkeySlotForCode(a_event->GetBSButtonCode());
                    if (slot < 12 && HasHotkeySlot(slot)) {
                        const auto activated = ActivateHotkeySlot(slot);
                        const_cast<RE::ButtonEvent*>(a_event)->handled =
                            RE::InputEvent::HANDLED_RESULT::kStop;
                        Log("PLAYER_INPUT_CAPTURED action=activate-hotkey-front slot=" +
                            std::to_string(slot) + " result=" +
                            std::to_string(activated ? 1 : 0));
                        return;
                    }
                }
                if (!a_event || !a_event->QJustPressed() || !IsFavoritesTrigger(a_event) ||
                    FavoritesMenu::IsOpen() ||
                    !IsGameplayFavoritesContext()) {
                    if (a_event && a_event->QJustPressed() && IsFavoritesTrigger(a_event) &&
                        !FavoritesMenu::IsOpen() && !IsGameplayFavoritesContext()) {
                        Log("INPUT_PASSTHROUGH reason=blocked-context");
                    }
                    return;
                }
                auto* event = const_cast<RE::ButtonEvent*>(a_event);
                if (FavoritesMenu::IsOpen() && IsFavoritesTrigger(a_event)) {
                    event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                    Log("MENU_INPUT favorites-key toggle-close");
                    FavoritesMenu::Close();
                    return;
                }
                event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                Log("INPUT_CAPTURED input=F/Favorites action=open-custom-menu");
                FavoritesMenu::Open();
            }
        };

        OpenInputHandler g_openInputHandler;

        class OpenPlayerInputHandler final : public RE::PlayerInputHandler
        {
        public:
            explicit OpenPlayerInputHandler(RE::PlayerControlsData& a_data) :
                RE::PlayerInputHandler(a_data)
            {}

            bool ShouldHandleEvent(const RE::InputEvent* a_event) override
            {
                if (!a_event) {
                    return false;
                }
                if (FavoritesMenu::IsOpen()) {
                    if (IsConsoleTrigger(a_event)) {
                        return false;
                    }
                    return (a_event->eventType == RE::INPUT_EVENT_TYPE::kButton ||
                        a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick) &&
                        a_event->device != RE::INPUT_DEVICE::kMouse;
                }
                if (a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                    return false;
                }
                const auto* button = a_event->As<RE::ButtonEvent>();
                if (!button) {
                    return false;
                }
                if (FavoritesMenu::IsOpen()) {
                    return IsFavoritesTrigger(button) || IsMenuCloseTrigger(button);
                }
                if (IsFavoritesTrigger(button)) {
                    return IsGameplayFavoritesContext() || IsVanillaFavoritesOpen();
                }
                if (IsDPadDirection(button) && IsGameplayFavoritesContext()) {
                    // Handle both configured actions here: action 0 is an
                    // intentional no-op, not a request to pass to vanilla.
                    return true;
                }
                if (button->device == RE::INPUT_DEVICE::kKeyboard && IsGameplayFavoritesContext()) {
                    const auto slot = HotkeySlotForCode(button->GetBSButtonCode());
                    return slot < 12 && HasHotkeySlot(slot);
                }
                return false;
            }

            void OnButtonEvent(const RE::ButtonEvent* a_event) override
            {
                if (!a_event) {
                    return;
                }
                if (FavoritesMenu::IsOpen()) {
                    if (a_event->device != RE::INPUT_DEVICE::kMouse &&
                        a_event->GetBSButtonCode() == RE::BS_BUTTON_CODE::kEscape) {
                        auto* event = const_cast<RE::ButtonEvent*>(a_event);
                        const bool justPressed = a_event->QJustPressed();
                        HardBlockFavoritesEscape(event, "player-controls");
                        if (justPressed) {
                            Log("MENU_INPUT close-key action=queue-hide");
                            FavoritesMenu::Close();
                        }
                        return;
                    }
                    if (!IsConsoleTrigger(a_event) && a_event->device != RE::INPUT_DEVICE::kMouse) {
                        FavoritesMenu::HandlePlayerButton(a_event);
                    }
                    return;
                }
                if (IsPipboyOpen() &&
                    IsQuickkeyUserEvent(a_event->QUserEvent().c_str())) {
                    HardBlockPipboyQuickkey(const_cast<RE::ButtonEvent*>(a_event),
                        "player-controls");
                    return;
                }
                if (IsDPadDirection(a_event) && IsGameplayFavoritesContext()) {
                    const auto action = DPadActionForInput(
                        a_event->QUserEvent().c_str(), a_event->GetBSButtonCode());
                    auto* event = const_cast<RE::ButtonEvent*>(a_event);
                    const bool justPressed = a_event->QJustPressed();
                    HardBlockGameplayDPad(event);
                    if (justPressed) {
                        if (action == 2) {
                            Log("PLAYER_INPUT_CAPTURED input=DPad action=open-custom-menu consumed=1");
                            FavoritesMenu::Open();
                        } else {
                            Log("PLAYER_INPUT_CAPTURED input=DPad action=no-action consumed=1");
                        }
                    }
                    return;
                }
                if (!a_event->QJustPressed()) {
                    return;
                }
                if (!IsFavoritesTrigger(a_event)) {
                    if (a_event->device == RE::INPUT_DEVICE::kKeyboard && IsGameplayFavoritesContext()) {
                        const auto slot = HotkeySlotForCode(a_event->GetBSButtonCode());
                        if (slot < 12 && ActivateHotkeySlot(slot)) {
                            auto* event = const_cast<RE::ButtonEvent*>(a_event);
                            event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                            Log("PLAYER_INPUT_CAPTURED action=activate-hotkey slot=" + std::to_string(slot));
                        }
                    }
                    return;
                }
                if (IsVanillaFavoritesOpen()) {
                    CloseVanillaFavoritesMenu();
                    auto* event = const_cast<RE::ButtonEvent*>(a_event);
                    event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                    Log("PLAYER_INPUT_CAPTURED input=F/Favorites action=replace-already-open-vanilla");
                    FavoritesMenu::Open();
                    return;
                }
                if (!IsGameplayFavoritesContext()) {
                    return;
                }
                auto* event = const_cast<RE::ButtonEvent*>(a_event);
                event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                Log("PLAYER_INPUT_CAPTURED input=F/Favorites action=open-custom-menu");
                FavoritesMenu::Open();
            }

            void OnThumbstickEvent(const RE::ThumbstickEvent* a_event) override
            {
                if (FavoritesMenu::IsOpen() && a_event && a_event->device != RE::INPUT_DEVICE::kMouse) {
                    FavoritesMenu::HandlePlayerThumbstick(a_event);
                }
            }
        };

        void Log(std::string_view a_message)
        {
            if (static_cast<std::uint8_t>(ClassifyLogLevel(a_message)) >
                static_cast<std::uint8_t>(ReadLogLevel())) {
                return;
            }
            REX::INFO("[AozoraFavoritesSWF] {}", a_message);
            static std::mutex logMutex;
            std::lock_guard lock(logMutex);
            std::ofstream file("Data/F4SE/Plugins/AozoraFavoritesSWF.log", std::ios::app);
            if (file.is_open()) {
                file << '[' << GetTickCount64() << "] " << a_message << '\n';
            }
        }

        void MixSnapshotHash(std::uint64_t& a_hash, std::uint64_t a_value)
        {
            a_hash ^= a_value + 0x9E3779B97F4A7C15ull +
                (a_hash << 6) + (a_hash >> 2);
            a_hash *= 0x100000001B3ull;
        }

        void MixSnapshotString(std::uint64_t& a_hash, std::string_view a_value)
        {
            for (const auto character : a_value) {
                MixSnapshotHash(a_hash, static_cast<unsigned char>(character));
            }
            MixSnapshotHash(a_hash, 0);
        }

        std::uint64_t SnapshotStateHash(
            const std::vector<FavoriteSnapshotEntry>& a_entries,
            std::uint32_t a_category,
            std::uint32_t a_selected,
            std::uint32_t a_focusSource,
            int a_iconMode,
            bool a_showItemInnerName,
            bool a_mascotEnabled,
            const ThemeSettings& a_theme,
            const LayoutSettings& a_layout)
        {
            std::uint64_t hash = 1469598103934665603ull;
            MixSnapshotHash(hash, a_category);
            MixSnapshotHash(hash, a_selected);
            MixSnapshotHash(hash, a_focusSource);
            MixSnapshotHash(hash, static_cast<std::uint64_t>(a_iconMode));
            MixSnapshotHash(hash, a_showItemInnerName ? 1 : 0);
            MixSnapshotHash(hash, a_mascotEnabled ? 1 : 0);
            MixSnapshotHash(hash, static_cast<std::uint64_t>(a_theme.mode));
            MixSnapshotHash(hash, static_cast<std::uint64_t>(a_theme.preset));
            MixSnapshotHash(hash, std::bit_cast<std::uint32_t>(a_theme.r));
            MixSnapshotHash(hash, std::bit_cast<std::uint32_t>(a_theme.g));
            MixSnapshotHash(hash, std::bit_cast<std::uint32_t>(a_theme.b));

            // SkyuiLikeLayout is composed exclusively of floats, including
            // the five float fields in each mascot entry. Hashing those
            // values keeps layout-editor changes visible to the cache.
            const auto* layoutValues = reinterpret_cast<const float*>(
                std::addressof(a_layout.skyuiLike16x9));
            constexpr auto layoutValueCount = sizeof(SkyuiLikeLayout) / sizeof(float);
            for (std::size_t index = 0; index < layoutValueCount; ++index) {
                MixSnapshotHash(hash, std::bit_cast<std::uint32_t>(layoutValues[index]));
            }
            MixSnapshotHash(hash, a_layout.editorMode ? 1 : 0);
            MixSnapshotHash(hash, std::bit_cast<std::uint32_t>(a_layout.refreshSeconds));

            for (const auto& entry : a_entries) {
                MixSnapshotHash(hash, entry.formID);
                MixSnapshotHash(hash, entry.instanceKey);
                MixSnapshotHash(hash, entry.hotkeySlot);
                MixSnapshotHash(hash, entry.count);
                MixSnapshotHash(hash, entry.equipped ? 1 : 0);
                MixSnapshotHash(hash, entry.useCount);
                MixSnapshotString(hash, entry.name);
                MixSnapshotString(hash, entry.type);
                MixSnapshotString(hash, entry.iconLibrary);
                MixSnapshotString(hash, entry.iconClass);
                MixSnapshotString(hash, entry.iconCategory);
            }
            return hash;
        }

        RE::BSFixedString MenuName()
        {
            return RE::BSFixedString(FavoritesMenu::MENU_NAME.data());
        }

        void ScheduleFavoriteActivation(std::uint32_t a_formID, std::uint64_t a_instanceKey)
        {
            std::thread([a_formID, a_instanceKey] {
                Sleep(30);
                if (auto* tasks = F4SE::GetTaskInterface()) {
                    tasks->AddTask([a_formID, a_instanceKey] {
                        const bool accepted = ActivateFavorite(a_formID, a_instanceKey);
                        Log("UI_ACTION activate accepted=" + std::to_string(accepted ? 1 : 0));
                        if (accepted) {
                            FavoritesMenu::RefreshSnapshotIfOpen();
                        }
                    });
                } else {
                    const bool accepted = ActivateFavorite(a_formID, a_instanceKey);
                    Log("UI_ACTION activate accepted=" + std::to_string(accepted ? 1 : 0));
                    if (accepted) {
                        FavoritesMenu::RefreshSnapshotIfOpen();
                    }
                }
            }).detach();
        }

        void ScheduleFavoriteRemoval(std::uint32_t a_formID, std::uint64_t a_instanceKey)
        {
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddTask([a_formID, a_instanceKey] {
                    const bool result = RemoveFavorite(a_formID, a_instanceKey);
                    FavoritesMenu::WriteLog("UI_ACTION remove result=" + std::to_string(result ? 1 : 0));
                    FavoritesMenu::RefreshSnapshotIfOpen();
                });
            }
        }

        void ScheduleHotkeyAssignment(
            std::uint32_t a_formID,
            std::uint64_t a_instanceKey,
            std::uint32_t a_slot)
        {
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddTask([a_formID, a_instanceKey, a_slot] {
                    const bool result = AssignHotkey(a_formID, a_instanceKey, a_slot);
                    FavoritesMenu::WriteLog("UI_ACTION assign-hotkey result=" + std::to_string(result ? 1 : 0));
                    FavoritesMenu::RefreshSnapshotIfOpen();
                });
            }
        }

        void SetGameplayHandlersEnabled(bool a_enabled)
        {
            auto* controls = RE::PlayerControls::GetSingleton();
            if (!controls) {
                return;
            }
            auto set = [a_enabled](auto* a_handler) {
                if (a_handler) {
                    reinterpret_cast<RE::PlayerInputHandler*>(a_handler)->inputEventHandlingEnabled = a_enabled;
                }
            };
            set(controls->lookHandler);
            set(controls->sprintHandler);
            set(controls->readyWeaponHandler);
            set(controls->autoMoveHandler);
            set(controls->toggleRunHandler);
            set(controls->activateHandler);
            set(controls->jumpHandler);
            set(controls->attackHandler);
            set(controls->runHandler);
            set(controls->sneakHandler);
            set(controls->togglePOVHandler);
            set(controls->meleeThrowHandler);
            set(controls->grabRotationHandler);
        }

    }

    bool ResolvePipboySelectionByIndex(
        std::uint32_t a_selectedIndex,
        std::uint32_t& a_formID,
        std::uint32_t& a_stackID)
    {
        return NativePipboySelectionByIndex(a_selectedIndex, a_formID, a_stackID);
    }

    namespace
    {
        void EnsureAozoraTranslations(RE::BSScaleformManager* a_scaleform)
        {
            static bool loaded = false;
            if (loaded || !a_scaleform || !a_scaleform->loader) {
                return;
            }
            auto* translator = a_scaleform->GetTranslator();
            if (!translator) {
                return;
            }
            translator->AddTranslationsMod("AozoraFavorites");
            translator->Release();
            loaded = true;
            Log("TRANSLATIONS loaded mod=AozoraFavorites");
        }
    }

    FavoritesMenu::FavoritesMenu()
    {
        categoryIndex_ = std::min(g_lastCategoryIndex, 3u);
        menuFlags.set(RE::UI_MENU_FLAGS::kModal);
        menuFlags.set(RE::UI_MENU_FLAGS::kUsesMenuContext);
        menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);
        menuFlags.set(RE::UI_MENU_FLAGS::kUpdateUsesCursor);
        menuFlags.set(RE::UI_MENU_FLAGS::kUsesMovementToDirection);
        menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving);
        depthPriority = RE::UI_DEPTH_PRIORITY::kStandard;
        inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kQuickContainerMenu;

        auto* scaleform = RE::BSScaleformManager::GetSingleton();
        EnsureAozoraTranslations(scaleform);
        Log("MENU_CODE_OBJECT mapping-start");
        MapCodeObjectFunctions();
        bool loaded = false;
        if (scaleform) {
            // Use the native LoadMovie entry first. Besides being the path
            // used by Bethesda's menus, it is the entry intercepted by F4SE
            // for installing the global root.f4se Scaleform object. Keep the
            // CommonLib LoadMovieEx path as a compatibility fallback for
            // runtimes where the direct entry rejects a loose SWF path.
            loaded = scaleform->LoadMovie(
                *this,
                uiMovie,
                MENU_NAME.data(),
                "root1.BGSCodeObj",
                Scaleform::GFx::Movie::ScaleModeType::kShowAll,
                0.0F);
            Log(std::string("MENU_SWF_LOAD direct=") + (loaded ? "ok" : "failed"));
            if (!loaded) {
                loaded = scaleform->LoadMovieEx(
                    *this,
                    MOVIE_PATH,
                    "root1.BGSCodeObj",
                    Scaleform::GFx::Movie::ScaleModeType::kShowAll,
                    0.0F);
                Log(std::string("MENU_SWF_LOAD ex=") + (loaded ? "ok" : "failed"));
            }
        }
        if (loaded) {
            movieLoaded_ = true;
            g_menu = this;
            Log("MENU_CODE_OBJECT mapping-complete path=root1.BGSCodeObj");
            InstallDirectCodeObjectFunctions();
            Scaleform::GFx::Value f4seObject;
            Scaleform::GFx::Value rootF4seObject;
            const bool f4seFound = uiMovie->GetVariable(
                std::addressof(f4seObject), "root1.f4se");
            const bool rootF4seFound = uiMovie->GetVariable(
                std::addressof(rootF4seObject), "root.f4se");
            const bool mountImageFound = f4seFound && f4seObject.IsObject() &&
                f4seObject.HasMember("MountImage");
            const bool rootMountImageFound = rootF4seFound && rootF4seObject.IsObject() &&
                rootF4seObject.HasMember("MountImage");
            Log("F4SE_SCALEFORM_OBJECT root1=" + std::to_string(f4seFound ? 1 : 0) +
                " object=" + std::to_string(f4seFound && f4seObject.IsObject() ? 1 : 0) +
                " mountImage=" + std::to_string(mountImageFound ? 1 : 0) +
                " root=" + std::to_string(rootF4seFound ? 1 : 0) +
                " rootMountImage=" + std::to_string(rootMountImageFound ? 1 : 0));
            Log("MENU_CONSTRUCTOR swf=loaded path=Interface/AozoraFavoritesMenu.swf");
        } else {
            Log("MENU_CONSTRUCTOR swf=load-failed path=Interface/AozoraFavoritesMenu.swf");
        }
    }

    void FavoritesMenu::WriteLog(std::string_view a_message)
    {
        Log(a_message);
    }

    FavoritesMenu::~FavoritesMenu()
    {
        EndSession();
        if (g_menu == this) {
            g_menu = nullptr;
        }
    }

    void FavoritesMenu::MapCodeObjectFunctions()
    {
        MapCodeMethodToASFunction("Initialize", static_cast<std::int32_t>(NativeFunction::kInitialize));
        MapCodeMethodToASFunction("Close", static_cast<std::int32_t>(NativeFunction::kClose));
        MapCodeMethodToASFunction("RequestPause", static_cast<std::int32_t>(NativeFunction::kRequestPause));
        MapCodeMethodToASFunction("RequestSnapshot", static_cast<std::int32_t>(NativeFunction::kRequestSnapshot));
        MapCodeMethodToASFunction("ActivateFavorite", static_cast<std::int32_t>(NativeFunction::kActivateFavorite));
        MapCodeMethodToASFunction("RemoveFavorite", static_cast<std::int32_t>(NativeFunction::kRemoveFavorite));
        MapCodeMethodToASFunction("AssignHotkey", static_cast<std::int32_t>(NativeFunction::kAssignHotkey));
        MapCodeMethodToASFunction("SelectCategory", static_cast<std::int32_t>(NativeFunction::kSelectCategory));
        MapCodeMethodToASFunction("WriteLog", static_cast<std::int32_t>(NativeFunction::kWriteLog));
    }

    void FavoritesMenu::InstallDirectCodeObjectFunctions()
    {
        if (!uiMovie) {
            return;
        }
        Scaleform::GFx::Value codeObject;
        if (!uiMovie->GetVariable(std::addressof(codeObject), "root1.BGSCodeObj") ||
            !codeObject.IsObject()) {
            Log("DIRECT_AS3_BIND failed root1.BGSCodeObj lookup");
            return;
        }
        constexpr std::array functions{
            std::pair{ "Initialize", NativeFunction::kInitialize },
            std::pair{ "Close", NativeFunction::kClose },
            std::pair{ "RequestPause", NativeFunction::kRequestPause },
            std::pair{ "RequestSnapshot", NativeFunction::kRequestSnapshot },
            std::pair{ "ActivateFavorite", NativeFunction::kActivateFavorite },
            std::pair{ "RemoveFavorite", NativeFunction::kRemoveFavorite },
            std::pair{ "AssignHotkey", NativeFunction::kAssignHotkey },
            std::pair{ "SelectFavorite", NativeFunction::kSelectFavorite },
            std::pair{ "SelectCategory", NativeFunction::kSelectCategory },
            std::pair{ "WriteLog", NativeFunction::kWriteLog }
        };
        for (const auto& [name, function] : functions) {
            auto* handler = new DirectNativeFunctionHandler();
            Scaleform::GFx::Value callback;
            uiMovie->CreateFunction(std::addressof(callback), handler,
                reinterpret_cast<void*>(static_cast<std::intptr_t>(function)));
            const bool bound = codeObject.SetMember(name, callback);
            handler->Release();
            Log(std::string("DIRECT_AS3_BIND ") + name + (bound ? "=ok" : "=failed"));
        }
    }

    void FavoritesMenu::Call(const Params& a_params)
    {
        const auto function = static_cast<NativeFunction>(reinterpret_cast<std::intptr_t>(a_params.userData));
        Log("AS3_CALL function=" + std::to_string(static_cast<std::int32_t>(function)) +
            " args=" + std::to_string(a_params.argCount));
        switch (function) {
        case NativeFunction::kInitialize:
            PushSnapshot();
            break;
        case NativeFunction::kClose:
            Close();
            break;
        case NativeFunction::kRequestPause:
            RequestPause();
            break;
        case NativeFunction::kRequestSnapshot:
            PushSnapshot();
            break;
        case NativeFunction::kActivateFavorite:
            {
                const auto formID = ParamUInt32(a_params, 0);
                const auto instanceKey = ParamUInt64(a_params, 1);
                Log("UI_ACTION activate queued form=" + std::to_string(formID));
                Close();
                ScheduleFavoriteActivation(formID, instanceKey);
            }
            break;
        case NativeFunction::kRemoveFavorite:
            {
                const auto formID = ParamUInt32(a_params, 0);
                const auto instanceKey = ParamUInt64(a_params, 1);
                Log("UI_ACTION remove queued form=" + std::to_string(formID));
                ScheduleFavoriteRemoval(formID, instanceKey);
            }
            break;
        case NativeFunction::kAssignHotkey:
            {
                const auto formID = ParamUInt32(a_params, 0);
                const auto instanceKey = ParamUInt64(a_params, 1);
                const auto slot = ParamUInt32(a_params, 2);
                Log("UI_ACTION assign-hotkey queued form=" + std::to_string(formID));
                ScheduleHotkeyAssignment(formID, instanceKey, slot);
            }
            break;
        case NativeFunction::kSelectFavorite:
            SelectIdentityNative(ParamUInt32(a_params, 0), ParamUInt64(a_params, 1));
            break;
        case NativeFunction::kSelectCategory:
            SelectCategoryNative(ParamUInt32(a_params, 0));
            break;
        case NativeFunction::kWriteLog:
            WriteLog(ParamString(a_params, 0));
            break;
        default:
            Log("Unknown AS3 native function");
            break;
        }
    }

    void FavoritesMenu::OnAddedToMenuStack()
    {
        RE::GameMenuBase::OnAddedToMenuStack();
        g_closeQueued = false;
        if (!movieLoaded_ || !uiMovie) {
            Log("MENU_STACK opening=true but swf-not-loaded; closing");
            Close();
            return;
        }
        BeginSession();
        g_customOpenRequested = false;
        InitializeStore();
        Log("MENU_STACK opening=true");
        PushSnapshot();
    }

    void FavoritesMenu::OnRemovedFromMenuStack()
    {
        EndSession();
        g_closeQueued = false;
        g_customOpenRequested = false;
        Log("MENU_STACK opening=false");
        RE::GameMenuBase::OnRemovedFromMenuStack();
    }

    bool FavoritesMenu::ShouldHandleEvent(const RE::InputEvent* a_event)
    {
        if (!a_event || !OnStack() || IsConsoleTrigger(a_event)) {
            return false;
        }
        return a_event->eventType == RE::INPUT_EVENT_TYPE::kButton ||
            a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick;
    }

    void FavoritesMenu::OnButtonEvent(const RE::ButtonEvent* a_event)
    {
        if (!a_event) {
            return;
        }

        const auto code = a_event->GetBSButtonCode();
        const bool close = a_event->QJustPressed() && IsMenuCloseTrigger(a_event);

        auto* event = const_cast<RE::ButtonEvent*>(a_event);
        if (close) {
            if (code == RE::BS_BUTTON_CODE::kEscape) {
                HardBlockFavoritesEscape(event, "favorites-menu");
            }
            event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
            Log("MENU_INPUT close-key action=queue-hide");
            if (code == RE::BS_BUTTON_CODE::kEscape) {
                RequestPause();
            } else {
                Close();
            }
            return;
        }

        // Keep keyboard/gamepad input inside the modal menu. Leave mouse
        // buttons available to the Scaleform movie for its own click events.
        if (a_event->device != RE::INPUT_DEVICE::kMouse &&
            a_event->QJustPressed() && IsFavoritesTrigger(a_event)) {
            event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
            Log("MENU_INPUT favorites-key toggle-close");
            Close();
            return;
        }
        if (a_event->device == RE::INPUT_DEVICE::kKeyboard && a_event->QJustPressed()) {
            focusSource_ = 1;
            const auto slot = HotkeySlotForCode(a_event->GetBSButtonCode());
            if (slot < 12) {
                AssignSelectionHotkey(slot);
                Log("MENU_INPUT assign-hotkey slot=" + std::to_string(slot));
            }
        }
        if (a_event->QJustPressed()) {
            const auto navCode = a_event->GetBSButtonCode();
            const auto focusSource = a_event->device == RE::INPUT_DEVICE::kGamepad ? 2u : 1u;
            if (navCode == RE::BS_BUTTON_CODE::kUp || navCode == RE::BS_BUTTON_CODE::kW ||
                navCode == RE::BS_BUTTON_CODE::kDPAD_Up) {
                MoveSelectionNative(-1, focusSource);
                Log("MENU_INPUT navigation=up native");
            } else if (navCode == RE::BS_BUTTON_CODE::kDown || navCode == RE::BS_BUTTON_CODE::kS ||
                navCode == RE::BS_BUTTON_CODE::kDPAD_Down) {
                MoveSelectionNative(1, focusSource);
                Log("MENU_INPUT navigation=down native");
            } else if (navCode == RE::BS_BUTTON_CODE::kLeft || navCode == RE::BS_BUTTON_CODE::kA ||
                navCode == RE::BS_BUTTON_CODE::kDPAD_Left) {
                MoveCategoryNative(-1, focusSource);
                Log("MENU_INPUT navigation=category-left native");
            } else if (navCode == RE::BS_BUTTON_CODE::kRight || navCode == RE::BS_BUTTON_CODE::kD ||
                navCode == RE::BS_BUTTON_CODE::kDPAD_Right) {
                MoveCategoryNative(1, focusSource);
                Log("MENU_INPUT navigation=category-right native");
            } else if (navCode == RE::BS_BUTTON_CODE::kE || navCode == RE::BS_BUTTON_CODE::kAButton) {
                ActivateSelectionNative();
                Log("MENU_INPUT action=activate native");
            } else if (navCode == RE::BS_BUTTON_CODE::kQ || navCode == RE::BS_BUTTON_CODE::kXButton) {
                RemoveSelectionNative();
                Log("MENU_INPUT action=remove native");
            }
        }
        if (a_event->device != RE::INPUT_DEVICE::kMouse) {
            event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
            // The custom menu already consumed keyboard/gamepad navigation.
            // Do not pass the same event through IMenu, otherwise Fallout's
            // movement handler can move the world camera until focus catches up.
            return;
        }
        RE::IMenu::OnButtonEvent(a_event);
    }

    void FavoritesMenu::OnThumbstickEvent(const RE::ThumbstickEvent* a_event)
    {
        if (!a_event || !uiMovie) {
            return;
        }
        auto* event = const_cast<RE::ThumbstickEvent*>(a_event);
        event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        if (a_event->QIDCode() != RE::ThumbstickEvent::kLeft ||
            a_event->prevDir == a_event->currDir || a_event->currDir == RE::DIRECTION_VAL::kNone) {
            return;
        }
        switch (a_event->currDir) {
        case RE::DIRECTION_VAL::kUp:
            MoveSelectionNative(-1, 2);
            break;
        case RE::DIRECTION_VAL::kDown:
            MoveSelectionNative(1, 2);
            break;
        case RE::DIRECTION_VAL::kLeft:
            MoveCategoryNative(-1, 2);
            break;
        case RE::DIRECTION_VAL::kRight:
            MoveCategoryNative(1, 2);
            break;
        default:
            break;
        }
    }

    void FavoritesMenu::PushStatus(const char* a_status)
    {
        if (!uiMovie || !a_status) {
            return;
        }
        Scaleform::GFx::Value status{ a_status };
        uiMovie->Invoke("root1.Menu_mc.ApplyStatus", nullptr, std::addressof(status), 1);
    }

    bool FavoritesMenu::UpdateSelectionVisual()
    {
        if (!uiMovie || !uiMovie->asMovieRoot) {
            return false;
        }
        Scaleform::GFx::Value arguments[2]{
            Scaleform::GFx::Value(selectedIndex_),
            Scaleform::GFx::Value(focusSource_)
        };
        Scaleform::GFx::Value result;
        const bool invoked = uiMovie->Invoke(
            "root1.Menu_mc.UpdateSelectionOnly", std::addressof(result), arguments, 2);
        const bool updated = invoked && result.IsBoolean() && result.GetBoolean();
        Log("SNAPSHOT_SELECTION_ONLY index=" + std::to_string(selectedIndex_) +
            " updated=" + std::to_string(updated ? 1 : 0));
        return updated;
    }

    std::vector<std::size_t> FavoritesMenu::VisibleEntryIndexes() const
    {
        std::vector<std::size_t> result;
        result.reserve(entries_.size());
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const auto& type = entries_[i].type;
            const bool visible = categoryIndex_ == 0 ||
                (categoryIndex_ == 1 && type == "WEAP") ||
                (categoryIndex_ == 2 && type == "ARMO") ||
                (categoryIndex_ == 3 && type == "ALCH");
            if (visible) {
                result.push_back(i);
            }
        }
        return result;
    }

    void FavoritesMenu::MoveSelectionNative(int a_delta, std::uint32_t a_focusSource)
    {
        focusSource_ = a_focusSource;
        const auto visible = VisibleEntryIndexes();
        if (visible.empty()) {
            selectedIndex_ = 0;
            return;
        }
        const auto count = static_cast<int>(visible.size());
        auto next = static_cast<int>(selectedIndex_) + a_delta;
        if (next < 0) {
            next = count - 1;
        }
        if (next >= count) {
            next = 0;
        }
        selectedIndex_ = static_cast<std::uint32_t>(next);
        Log("NATIVE_SELECTION index=" + std::to_string(selectedIndex_) +
            " category=" + std::to_string(categoryIndex_));
        if (!UpdateSelectionVisual()) {
            // Scrolling to a row outside the current viewport and switching
            // keyboard/gamepad hint sets still require the full snapshot.
            PushSnapshot();
        }
    }

    void FavoritesMenu::MoveCategoryNative(int a_delta, std::uint32_t a_focusSource)
    {
        focusSource_ = a_focusSource;
        auto next = static_cast<int>(categoryIndex_) + a_delta;
        if (next < 0) {
            next = 3;
        }
        if (next > 3) {
            next = 0;
        }
        categoryIndex_ = static_cast<std::uint32_t>(next);
        g_lastCategoryIndex = categoryIndex_;
        selectedIndex_ = 0;
        Log("NATIVE_CATEGORY index=" + std::to_string(categoryIndex_));
        PushSnapshot();
    }

    void FavoritesMenu::ActivateSelectionNative()
    {
        const auto visible = VisibleEntryIndexes();
        if (visible.empty() || selectedIndex_ >= visible.size()) {
            Log("NATIVE_ACTION activate skipped=no-selection");
            return;
        }
        const auto& entry = entries_[visible[selectedIndex_]];
        Log("NATIVE_ACTION activate form=" + std::to_string(entry.formID));
        Close();
        ScheduleFavoriteActivation(entry.formID, entry.instanceKey);
    }

    void FavoritesMenu::RemoveSelectionNative()
    {
        const auto visible = VisibleEntryIndexes();
        if (visible.empty() || selectedIndex_ >= visible.size()) {
            Log("NATIVE_ACTION remove skipped=no-selection");
            return;
        }
        const auto& entry = entries_[visible[selectedIndex_]];
        Log("NATIVE_ACTION remove form=" + std::to_string(entry.formID));
        ScheduleFavoriteRemoval(entry.formID, entry.instanceKey);
    }

    void FavoritesMenu::AssignSelectionHotkey(std::uint32_t a_slot)
    {
        const auto visible = VisibleEntryIndexes();
        if (a_slot >= 12 || visible.empty() || selectedIndex_ >= visible.size()) {
            return;
        }
        const auto& entry = entries_[visible[selectedIndex_]];
        Log("NATIVE_ACTION assign-hotkey form=" + std::to_string(entry.formID) +
            " slot=" + std::to_string(a_slot));
        ScheduleHotkeyAssignment(entry.formID, entry.instanceKey, a_slot);
    }

    void FavoritesMenu::SelectCategoryNative(std::uint32_t a_category)
    {
        if (a_category > 3) {
            return;
        }
        categoryIndex_ = a_category;
        g_lastCategoryIndex = categoryIndex_;
        selectedIndex_ = 0;
        focusSource_ = 3;
        Log("NATIVE_CATEGORY mouse index=" + std::to_string(categoryIndex_));
        PushSnapshot();
    }

    void FavoritesMenu::SelectIdentityNative(std::uint32_t a_formID, std::uint64_t a_instanceKey)
    {
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const auto& entry = entries_[i];
            if (entry.formID != a_formID || entry.instanceKey != a_instanceKey) {
                continue;
            }
            const auto visible = VisibleEntryIndexes();
            for (std::size_t selected = 0; selected < visible.size(); ++selected) {
                if (visible[selected] == i) {
                    selectedIndex_ = static_cast<std::uint32_t>(selected);
                    focusSource_ = 3;
                    Log("NATIVE_SELECTION mouse index=" + std::to_string(selectedIndex_));
                    return;
                }
            }
        }
    }

    void FavoritesMenu::PushSnapshot()
    {
        if (!uiMovie || !uiMovie->asMovieRoot) {
            return;
        }
        if (categoryIndex_ > 3) {
            categoryIndex_ = 0;
        }
        g_lastCategoryIndex = categoryIndex_;
        entries_ = Snapshot();
        const auto visible = VisibleEntryIndexes();
        if (visible.empty()) {
            selectedIndex_ = 0;
        } else if (selectedIndex_ >= visible.size()) {
            selectedIndex_ = static_cast<std::uint32_t>(visible.size() - 1);
        }
        const auto iconMode = ReadIconMode();
        const auto showItemInnerName = ReadShowItemInnerName();
        const auto mascotEnabled = ReadMascotEnabled();
        const auto themeSettings = ReadThemeSettings();
        const auto layoutSettings = ReadLayoutSettings();
        const auto entryHash = SnapshotStateHash(
            entries_, categoryIndex_, selectedIndex_, focusSource_, iconMode,
            showItemInnerName, mascotEnabled, themeSettings, layoutSettings);
        if (snapshotCacheValid_ && lastCategory_ == categoryIndex_ &&
            lastSelected_ == selectedIndex_ && lastEntryHash_ == entryHash) {
            Log("SNAPSHOT_SKIP category=" + std::to_string(categoryIndex_) +
                " selected=" + std::to_string(selectedIndex_) +
                " hash=" + std::to_string(entryHash));
            return;
        }
        Scaleform::GFx::Value snapshot;
        Scaleform::GFx::Value items;
        uiMovie->CreateArray(std::addressof(items));
        for (const auto& entry : entries_) {
            Scaleform::GFx::Value item;
            uiMovie->CreateObject(std::addressof(item));
            item.SetMember("formID", Scaleform::GFx::Value(entry.formID));

            char instanceKey[24]{};
            std::snprintf(instanceKey, sizeof(instanceKey), "%016llX",
                static_cast<unsigned long long>(entry.instanceKey));
            item.SetMember("instanceKey", Scaleform::GFx::Value(instanceKey));

            item.SetMember("hotkey", Scaleform::GFx::Value(entry.hotkeySlot == 0xFFFFFFFFu ? -1 :
                static_cast<std::int32_t>(entry.hotkeySlot)));
            Scaleform::GFx::Value name;
            uiMovie->asMovieRoot->CreateString(std::addressof(name), entry.name.c_str());
            item.SetMember("name", name);
            Scaleform::GFx::Value type;
            uiMovie->asMovieRoot->CreateString(std::addressof(type), entry.type.c_str());
            item.SetMember("type", type);
            item.SetMember("count", Scaleform::GFx::Value(entry.count));
            item.SetMember("equipped", Scaleform::GFx::Value(entry.equipped));
            Scaleform::GFx::Value iconLibrary;
            uiMovie->asMovieRoot->CreateString(std::addressof(iconLibrary), entry.iconLibrary.c_str());
            item.SetMember("iconLibrary", iconLibrary);
            Scaleform::GFx::Value iconClass;
            uiMovie->asMovieRoot->CreateString(std::addressof(iconClass), entry.iconClass.c_str());
            item.SetMember("iconClass", iconClass);
            Scaleform::GFx::Value iconCategory;
            uiMovie->asMovieRoot->CreateString(std::addressof(iconCategory), entry.iconCategory.c_str());
            item.SetMember("iconCategory", iconCategory);
            Scaleform::GFx::Value fallbackIconType;
            uiMovie->asMovieRoot->CreateString(std::addressof(fallbackIconType), entry.iconCategory.c_str());
            item.SetMember("fallbackIconType", fallbackIconType);
            item.SetMember("useCount", Scaleform::GFx::Value(entry.useCount));
            items.PushBack(item);
        }
        uiMovie->CreateObject(std::addressof(snapshot));
        snapshot.SetMember("items", items);
        snapshot.SetMember("categoryIndex", Scaleform::GFx::Value(categoryIndex_));
        snapshot.SetMember("selectedIndex", Scaleform::GFx::Value(selectedIndex_));
        snapshot.SetMember("focusSource", Scaleform::GFx::Value(focusSource_));
        snapshot.SetMember("iconMode", Scaleform::GFx::Value(iconMode));
        snapshot.SetMember("showItemInnerName", Scaleform::GFx::Value(showItemInnerName));
        snapshot.SetMember("mascotEnabled", Scaleform::GFx::Value(mascotEnabled));

        Scaleform::GFx::Value theme;
        uiMovie->CreateObject(std::addressof(theme));
        theme.SetMember("mode", Scaleform::GFx::Value(themeSettings.mode));
        theme.SetMember("preset", Scaleform::GFx::Value(themeSettings.preset));
        theme.SetMember("r", Scaleform::GFx::Value(themeSettings.r));
        theme.SetMember("g", Scaleform::GFx::Value(themeSettings.g));
        theme.SetMember("b", Scaleform::GFx::Value(themeSettings.b));
        snapshot.SetMember("theme", theme);

        Scaleform::GFx::Value layout;
        uiMovie->CreateObject(std::addressof(layout));
        layout.SetMember("editorMode", Scaleform::GFx::Value(layoutSettings.editorMode));
        layout.SetMember("refreshSeconds", Scaleform::GFx::Value(layoutSettings.refreshSeconds));
        const auto& source = layoutSettings.skyuiLike16x9;
        Scaleform::GFx::Value currentLayout;
        uiMovie->CreateObject(std::addressof(currentLayout));
        currentLayout.SetMember("editorMode", Scaleform::GFx::Value(layoutSettings.editorMode));
        currentLayout.SetMember("refreshSeconds", Scaleform::GFx::Value(layoutSettings.refreshSeconds));
        const auto setFloat = [&](const char* key, float value) {
            currentLayout.SetMember(key, Scaleform::GFx::Value(value));
        };
        setFloat("panelX", source.panelX);
        setFloat("panelY", source.panelY);
        setFloat("panelScale", source.panelScale);
        setFloat("width", source.width);
        setFloat("height", source.height);
        setFloat("contentX", source.contentX);
        setFloat("contentY", source.contentY);
        setFloat("contentScaleX", source.contentScaleX);
        setFloat("contentScaleY", source.contentScaleY);
        setFloat("titleBlockX", source.titleBlockX);
        setFloat("titleBlockY", source.titleBlockY);
        setFloat("categoryBlockX", source.categoryBlockX);
        setFloat("categoryBlockY", source.categoryBlockY);
        setFloat("headerBlockX", source.headerBlockX);
        setFloat("headerBlockY", source.headerBlockY);
        setFloat("listBlockX", source.listBlockX);
        setFloat("listBlockY", source.listBlockY);
        setFloat("footerBlockX", source.footerBlockX);
        setFloat("footerBlockY", source.footerBlockY);
        setFloat("titleX", source.titleX);
        setFloat("titleY", source.titleY);
        setFloat("titleFontSize", source.titleFontSize);
        setFloat("categoryX", source.categoryX);
        setFloat("categoryY", source.categoryY);
        setFloat("categoryStep", source.categoryStep);
        setFloat("categoryWidth", source.categoryWidth);
        setFloat("categoryFontSize", source.categoryFontSize);
        setFloat("categoryHitX", source.categoryHitX);
        setFloat("categoryHitY", source.categoryHitY);
        setFloat("categoryHitWidth", source.categoryHitWidth);
        setFloat("categoryHitHeight", source.categoryHitHeight);
        setFloat("headerY", source.headerY);
        setFloat("headerNameX", source.headerNameX);
        setFloat("headerHotkeyX", source.headerHotkeyX);
        setFloat("headerQuantityX", source.headerQuantityX);
        setFloat("headerFontSize", source.headerFontSize);
        setFloat("listX", source.listX);
        setFloat("listTop", source.listTop);
        setFloat("rowHeight", source.rowHeight);
        setFloat("rowWidth", source.rowWidth);
        setFloat("rowIconX", source.rowIconX);
        setFloat("rowIconY", source.rowIconY);
        setFloat("rowIconScale", source.rowIconScale);
        setFloat("rowTextBlockY", source.rowTextBlockY);
        setFloat("rowNameX", source.rowNameX);
        setFloat("rowNameY", source.rowNameY);
        setFloat("rowNameWidth", source.rowNameWidth);
        setFloat("rowFontSize", source.rowFontSize);
        setFloat("nameScrollDelay", source.nameScrollDelay);
        setFloat("nameScrollSpeed", source.nameScrollSpeed);
        setFloat("nameScrollEndPause", source.nameScrollEndPause);
        setFloat("focusFrameX", source.focusFrameX);
        setFloat("focusFrameY", source.focusFrameY);
        setFloat("focusFrameWidth", source.focusFrameWidth);
        setFloat("focusFrameHeight", source.focusFrameHeight);
        setFloat("focusFrameThickness", source.focusFrameThickness);
        setFloat("focusFrameAlpha", source.focusFrameAlpha);
        setFloat("rowHotkeyX", source.rowHotkeyX);
        setFloat("rowHotkeyY", source.rowHotkeyY);
        setFloat("rowHotkeyWidth", source.rowHotkeyWidth);
        setFloat("rowHotkeyHeight", source.rowHotkeyHeight);
        setFloat("rowHotkeyScale", source.rowHotkeyScale);
        setFloat("rowHotkeyTextY", source.rowHotkeyTextY);
        setFloat("rowHotkeyFontSize", source.rowHotkeyFontSize);
        setFloat("rowQuantityX", source.rowQuantityX);
        setFloat("rowQuantityY", source.rowQuantityY);
        setFloat("rowQuantityWidth", source.rowQuantityWidth);
        setFloat("rowQuantityFontSize", source.rowQuantityFontSize);
        setFloat("scrollbarX", source.scrollbarX);
        setFloat("scrollbarY", source.scrollbarY);
        setFloat("scrollbarWidth", source.scrollbarWidth);
        setFloat("scrollbarHeight", source.scrollbarHeight);
        setFloat("scrollbarThumbMinHeight", source.scrollbarThumbMinHeight);
        setFloat("footerX", source.footerX);
        setFloat("footerY", source.footerY);
        setFloat("footerWidth", source.footerWidth);
        setFloat("footerHeight", source.footerHeight);
        setFloat("footerKey1X", source.footerKey1X);
        setFloat("footerKey1Y", source.footerKey1Y);
        setFloat("footerKey2X", source.footerKey2X);
        setFloat("footerKey2Y", source.footerKey2Y);
        setFloat("footerKey1TextY", source.footerKey1TextY);
        setFloat("footerKey2TextY", source.footerKey2TextY);
        setFloat("footerLabel1X", source.footerLabel1X);
        setFloat("footerLabel1Y", source.footerLabel1Y);
        setFloat("footerLabel2X", source.footerLabel2X);
        setFloat("footerLabel2Y", source.footerLabel2Y);
        setFloat("footerDividerX", source.footerDividerX);
        setFloat("footerDividerY", source.footerDividerY);
        setFloat("footerDividerHeight", source.footerDividerHeight);
        setFloat("footerKeyGap", source.footerKeyGap);
        setFloat("footerKeyWidth", source.footerKeyWidth);
        setFloat("footerKeyHeight", source.footerKeyHeight);
        setFloat("footerKeyScale", source.footerKeyScale);
        setFloat("footerKeyTextY", source.footerKeyTextY);
        setFloat("footerLabelXOffset", source.footerLabelXOffset);
        setFloat("footerLabelY", source.footerLabelY);
        setFloat("footerBottomLineY", source.footerBottomLineY);
        setFloat("footerDetailY", source.footerDetailY);
        setFloat("footerKeyFontSize", source.footerKeyFontSize);
        setFloat("footerLabelFontSize", source.footerLabelFontSize);
        setFloat("topMicroFontSize", source.topMicroFontSize);
        setFloat("topMicroAlpha", source.topMicroAlpha);
        setFloat("footerDetailFontSize", source.footerDetailFontSize);
        setFloat("footerDetailAlpha", source.footerDetailAlpha);
        setFloat("borderAlpha", source.borderAlpha);
        setFloat("outerHorizontalAlpha", source.outerHorizontalAlpha);
        setFloat("outerVerticalAlpha", source.outerVerticalAlpha);
        setFloat("innerHorizontalAlpha", source.innerHorizontalAlpha);
        setFloat("innerVerticalAlpha", source.innerVerticalAlpha);
        setFloat("frameCornerRadius", source.frameCornerRadius);
        setFloat("backgroundAlpha", source.backgroundAlpha);
        setFloat("rowAlpha", source.rowAlpha);
        setFloat("dividerAlpha", source.dividerAlpha);
        setFloat("scanlineAlpha", source.scanlineAlpha);
        setFloat("silhouetteAlpha", source.silhouetteAlpha);
        setFloat("silhouetteX", source.silhouetteX);
        setFloat("silhouetteY", source.silhouetteY);
        setFloat("silhouetteScale", source.silhouetteScale);
        setFloat("mascotBackdropX", source.mascotBackdropX);
        setFloat("mascotBackdropY", source.mascotBackdropY);
        setFloat("mascotBackdropScale", source.mascotBackdropScale);
        setFloat("mascotBackdropAlpha", source.mascotBackdropAlpha);

        Scaleform::GFx::Value mascots;
        uiMovie->CreateArray(std::addressof(mascots));
        for (std::size_t i = 0; i < source.mascots.size(); ++i) {
            Scaleform::GFx::Value mascot;
            uiMovie->CreateObject(std::addressof(mascot));
            mascot.SetMember("group", Scaleform::GFx::Value(MASCOT_SERIES_NAMES[i].data()));
            mascot.SetMember("x", Scaleform::GFx::Value(source.mascots[i].x));
            mascot.SetMember("y", Scaleform::GFx::Value(source.mascots[i].y));
            mascot.SetMember("scale", Scaleform::GFx::Value(source.mascots[i].scale));
            mascot.SetMember("rotation", Scaleform::GFx::Value(source.mascots[i].rotation));
            mascot.SetMember("alpha", Scaleform::GFx::Value(source.mascots[i].alpha));
            mascots.PushBack(mascot);
        }
        currentLayout.SetMember("mascots", mascots);
        layout.SetMember("current", currentLayout);
        snapshot.SetMember("layout", layout);
        const bool invoked = uiMovie->Invoke("root1.Menu_mc.ApplySnapshot", nullptr, std::addressof(snapshot), 1);
        if (invoked) {
            snapshotCacheValid_ = true;
            lastCategory_ = categoryIndex_;
            lastSelected_ = selectedIndex_;
            lastEntryHash_ = entryHash;
        }
        Log("SNAPSHOT_PUSH entries=" + std::to_string(entries_.size()) +
            " category=" + std::to_string(categoryIndex_) +
            " selected=" + std::to_string(selectedIndex_) +
            " invoke=" + std::to_string(invoked ? 1 : 0));
    }

    void FavoritesMenu::BeginSession()
    {
        if (sessionActive_) {
            return;
        }
        sessionActive_ = true;
        sessionID_ = ++g_menuSessionCounter;
        snapshotCacheValid_ = false;
        Log("MENU_SESSION_START id=" + std::to_string(sessionID_));
        SetGameplayHandlersEnabled(false);
        auto* timer = RE::BSTimer::GetSingleton();
        auto* main = RE::Main::GetSingleton();
        if (!timeCaptured_) {
            previousTimeMultiplier_ = RE::BSTimer::QGlobalTimeMultiplier();
            previousFreezeTime_ = main ? main->freezeTime : false;
            timeCaptured_ = true;
        }
        const auto configuredScale = ReadSlowMotionScale();
        if (configuredScale <= 0.0001F) {
            if (main) {
                main->freezeTime = true;
            }
            if (timer) {
                timer->SetGlobalTimeMultiplier(0.0F, true);
            }
            Log("session started; time mode=freeze scale=0");
        } else {
            if (main) {
                main->freezeTime = previousFreezeTime_;
            }
            if (timer) {
                const auto previous = previousTimeMultiplier_ > 0.0F ? previousTimeMultiplier_ : 1.0F;
                const auto target = std::min(previous, configuredScale);
                timer->SetGlobalTimeMultiplier(target, true);
            }
            Log("session started; time mode=slow scale=" + std::to_string(configuredScale));
        }
    }

    void FavoritesMenu::EndSession()
    {
        if (!sessionActive_ && !timeCaptured_) {
            return;
        }
        sessionActive_ = false;
        SetGameplayHandlersEnabled(true);
        if (timeCaptured_) {
            if (auto* timer = RE::BSTimer::GetSingleton()) {
                timer->SetGlobalTimeMultiplier(
                    previousTimeMultiplier_ > 0.0F ? previousTimeMultiplier_ : 1.0F,
                    true);
            }
            if (auto* main = RE::Main::GetSingleton()) {
                main->freezeTime = previousFreezeTime_;
            }
            timeCaptured_ = false;
        }
        Log("MENU_SESSION_END id=" + std::to_string(sessionID_));
        Log("session ended; time restored");
    }

    void FavoritesMenu::RequestPause()
    {
        // The favorites menu owns the close input. ESC closes this menu only;
        // it must not open the native pause menu as a side effect.
        Close();
    }

    RE::IMenu* FavoritesMenu::Create(const RE::UIMessage&)
    {
        return new FavoritesMenu();
    }

    void FavoritesMenu::Register()
    {
        if (g_registered) {
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        if (!ui || ui->GetMenu(MenuName())) {
            Log("MENU_REGISTER skipped ui-missing-or-name-already-present");
            return;
        }
        ui->RegisterMenu(MENU_NAME.data(), Create);
        if (!g_vanillaMenuSinkRegistered) {
            ui->RegisterSink<RE::MenuOpenCloseEvent>(std::addressof(g_vanillaMenuSink));
            g_vanillaMenuSinkRegistered = true;
            Log("vanilla FavoritesMenu open-close sink registered");
        }
        g_registered = true;
        Log("menu registered");
    }

    void FavoritesMenu::RegisterInput()
    {
        if (g_inputRegistrationStarted.exchange(true)) {
            return;
        }
        std::thread([] {
            Log("INPUT_REGISTRATION_THREAD started");
            std::uint32_t attempt = 0;
            for (;;) {
                if (!g_menuInputRegistered.load()) {
                    if (auto* controls = RE::MenuControls::GetSingleton()) {
                        controls->handlers.insert(controls->handlers.begin(), std::addressof(g_openInputHandler));
                        g_menuInputRegistered = true;
                        Log("Favorites input handler registered at front of MenuControls");
                    }
                }
                if (!g_playerInputRegistered.load()) {
                    if (auto* controls = RE::PlayerControls::GetSingleton()) {
                        g_playerInputHandler = new OpenPlayerInputHandler(controls->data);
                        controls->RegisterHandler(g_playerInputHandler);
                        for (auto it = controls->handlers.begin(); it != controls->handlers.end(); ++it) {
                            if (*it == g_playerInputHandler) {
                                controls->handlers.erase(it);
                                controls->handlers.insert(controls->handlers.begin(), g_playerInputHandler);
                                break;
                            }
                        }
                        g_playerInputRegistered = true;
                        Log("Favorites player input handler registered at front of PlayerControls");
                    }
                }
                if (g_menuInputRegistered.load() && g_playerInputRegistered.load()) {
                    Log("INPUT_REGISTRATION_THREAD complete");
                    return;
                }
                Sleep(100);
                ++attempt;
                if ((attempt % 600) == 0) {
                    Log("INPUT_REGISTRATION_THREAD still-waiting-for-game-input-singletons");
                }
            }
        }).detach();
    }

    void FavoritesMenu::InstallFavoritesInputHook()
    {
        InstallPipboyNativeInputHook();
        if (g_favoritesHookInstalled) {
            return;
        }
        REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::FavoritesManager[1] };
        g_originalFavoritesShouldHandleEvent = reinterpret_cast<FavoritesShouldHandleEvent_t>(
            vtable.write_vfunc(1, FavoritesManagerShouldHandleEventHook));
        g_favoritesHookInstalled = true;
        Log("vanilla FavoritesManager input hook installed");
    }

    void FavoritesMenu::Open()
    {
        Register();
        if (IsOpen()) {
            return;
        }
        g_customOpenRequested = true;
        CloseVanillaFavoritesMenu();
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            Log("MENU_OPEN queue-show");
            queue->AddMessage(MenuName(), RE::UI_MESSAGE_TYPE::kShow);
        } else {
            Log("MENU_OPEN failed message-queue-missing");
        }
    }

    void FavoritesMenu::Close()
    {
        if (!IsOpen() || g_closeQueued.exchange(true)) {
            return;
        }
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            Log("MENU_CLOSE queue-hide");
            queue->AddMessage(MenuName(), RE::UI_MESSAGE_TYPE::kHide);
        }
    }

    bool FavoritesMenu::IsOpen()
    {
        auto* ui = RE::UI::GetSingleton();
        return ui && ui->GetMenuOpen(MenuName());
    }

    void FavoritesMenu::RefreshSnapshotIfOpen()
    {
        if (g_menu && IsOpen()) {
            g_menu->PushSnapshot();
        }
    }

    void FavoritesMenu::HandlePlayerButton(const RE::ButtonEvent* a_event)
    {
        if (g_menu && IsOpen() && a_event) {
            g_menu->OnButtonEvent(a_event);
        }
    }

    void FavoritesMenu::HandlePlayerThumbstick(const RE::ThumbstickEvent* a_event)
    {
        if (g_menu && IsOpen() && a_event) {
            g_menu->OnThumbstickEvent(a_event);
        }
    }
}

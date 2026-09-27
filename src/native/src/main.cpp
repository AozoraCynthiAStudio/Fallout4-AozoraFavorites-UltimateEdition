#include <algorithm>
#include <variant>

#include "F4SE/F4SE.h"
#include "RE/B/BSScript_IVirtualMachine.h"
#include "RE/B/BSScriptUtil.h"
#include "RE/T/TESBoundObject.h"
#include "Aozora/ScaleformMenu.h"

#define AOZORA_SWF_EXPORT extern "C" [[maybe_unused]] __declspec(dllexport)

namespace
{
    bool IsMenuAvailable(std::monostate)
    {
        return true;
    }

    bool OpenMenu(std::monostate)
    {
        Aozora::SWF::FavoritesMenu::Open();
        return true;
    }

    void CloseMenu(std::monostate)
    {
        Aozora::SWF::FavoritesMenu::Close();
    }

    bool IsMenuOpen(std::monostate)
    {
        return Aozora::SWF::FavoritesMenu::IsOpen();
    }

    bool AddFavorite(std::monostate, RE::TESForm* a_form)
    {
        auto* object = a_form ? a_form->As<RE::TESBoundObject>() : nullptr;
        return object && Aozora::SWF::AddFavorite(object->GetFormID());
    }

    bool RemoveFavorite(std::monostate, RE::TESForm* a_form)
    {
        auto* object = a_form ? a_form->As<RE::TESBoundObject>() : nullptr;
        return object && Aozora::SWF::RemoveFavorite(object->GetFormID());
    }

    bool IsFavorite(std::monostate, RE::TESForm* a_form)
    {
        auto* object = a_form ? a_form->As<RE::TESBoundObject>() : nullptr;
        if (!object) {
            return false;
        }
        const auto entries = Aozora::SWF::Snapshot();
        return std::any_of(entries.begin(), entries.end(), [&](const auto& a_entry) {
            return a_entry.formID == object->GetFormID();
        });
    }

    bool AssignHotkey(std::monostate, RE::TESForm* a_form, std::int32_t a_slot)
    {
        auto* object = a_form ? a_form->As<RE::TESBoundObject>() : nullptr;
        return object && a_slot >= 0 && a_slot < 12 &&
            Aozora::SWF::AssignHotkey(object->GetFormID(), 0, static_cast<std::uint32_t>(a_slot));
    }

    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
    {
        if (!a_vm) {
            return false;
        }
        a_vm->BindNativeMethod("AozoraMenuNative", "IsMenuAvailable", IsMenuAvailable);
        a_vm->BindNativeMethod("AozoraMenuNative", "OpenMenu", OpenMenu);
        a_vm->BindNativeMethod("AozoraMenuNative", "CloseMenu", CloseMenu);
        a_vm->BindNativeMethod("AozoraMenuNative", "IsMenuOpen", IsMenuOpen);
        a_vm->BindNativeMethod("AozoraMenuNative", "AddFavorite", AddFavorite);
        a_vm->BindNativeMethod("AozoraMenuNative", "RemoveFavorite", RemoveFavorite);
        a_vm->BindNativeMethod("AozoraMenuNative", "IsFavorite", IsFavorite);
        a_vm->BindNativeMethod("AozoraMenuNative", "AssignHotkey", AssignHotkey);
        return true;
    }

    void F4SEAPI SaveCallback(const F4SE::SerializationInterface* a_interface)
    {
        Aozora::SWF::Save(a_interface);
    }

    void F4SEAPI LoadCallback(const F4SE::SerializationInterface* a_interface)
    {
        Aozora::SWF::Load(a_interface);
    }

    void F4SEAPI RevertCallback(const F4SE::SerializationInterface* a_interface)
    {
        Aozora::SWF::Revert(a_interface);
    }

    void MessageHandler(F4SE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }
        if (a_message->type == F4SE::MessagingInterface::kGameDataReady ||
            a_message->type == F4SE::MessagingInterface::kInputLoaded ||
            a_message->type == F4SE::MessagingInterface::kGameLoaded ||
            a_message->type == F4SE::MessagingInterface::kPostLoadGame) {
            Aozora::SWF::FavoritesMenu::Register();
            Aozora::SWF::FavoritesMenu::RegisterInput();
            Aozora::SWF::FavoritesMenu::InstallFavoritesInputHook();
            Aozora::SWF::InstallAlchemyActionTraceHook();
            Aozora::SWF::InstallAlchemyEquipTraceHook();
            Aozora::SWF::RegisterFavoriteChangeSink();
        }
        if (a_message->type == F4SE::MessagingInterface::kPostLoadGame) {
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddTask([] { Aozora::SWF::ClearVanillaFavorites(); });
            } else {
                Aozora::SWF::ClearVanillaFavorites();
            }
        }
    }
}

AOZORA_SWF_EXPORT F4SE::PluginVersionData F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData version{};
    version.PluginVersion({ 2, 1, 0, 0 });
    version.PluginName("AozoraFavoritesSWF");
    version.AuthorName("Aozora");
    version.UsesAddressLibrary(true);
    version.UsesAddressLibraryNG(true);
    version.UsesSigScanning(false);
    version.IsLayoutDependent(true);
    version.IsLayoutDependentNG(true);
    version.HasNoStructUse(false);
    version.CompatibleVersions({
        F4SE::RUNTIME_1_10_162, F4SE::RUNTIME_1_10_163,
        F4SE::RUNTIME_1_11_221, F4SE::RUNTIME_1_11_240
    });
    return version;
}();

AOZORA_SWF_EXPORT bool F4SEAPI F4SEPlugin_Query(
    const F4SE::QueryInterface* a_interface, F4SE::PluginInfo* a_info)
{
    if (!a_interface || !a_info) {
        return false;
    }
    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name = "AozoraFavoritesSWF";
    a_info->version = 1;
    const auto runtime = a_interface->RuntimeVersion();
    return !a_interface->IsEditor() &&
        (runtime == F4SE::RUNTIME_1_10_163 ||
            runtime == F4SE::RUNTIME_1_11_240);
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_interface)
{
    F4SE::Init(a_interface, F4SE::InitInfo{
        .trampoline = true,
        .trampolineSize = 128
    });
    Aozora::SWF::FavoritesMenu::WriteLog("PLUGIN_LOAD build=skyui_like_v1n");
    if (auto* messaging = F4SE::GetMessagingInterface()) {
        messaging->RegisterListener(MessageHandler);
    }
    if (auto* papyrus = F4SE::GetPapyrusInterface()) {
        papyrus->Register(RegisterPapyrus);
    }
    if (auto* serialization = F4SE::GetSerializationInterface()) {
        serialization->SetUniqueID('AoFM');
        serialization->SetSaveCallback(SaveCallback);
        serialization->SetLoadCallback(LoadCallback);
        serialization->SetRevertCallback(RevertCallback);
        Aozora::SWF::FavoritesMenu::WriteLog("STORE_SERIALIZATION callbacks-registered");
    }
    Aozora::SWF::FavoritesMenu::Register();
    Aozora::SWF::FavoritesMenu::RegisterInput();
    Aozora::SWF::FavoritesMenu::InstallFavoritesInputHook();
    Aozora::SWF::InstallAlchemyActionTraceHook();
    Aozora::SWF::InstallAlchemyEquipTraceHook();
    return true;
}

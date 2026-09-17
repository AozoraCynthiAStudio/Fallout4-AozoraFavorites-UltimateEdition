#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "F4SE/F4SE.h"

namespace Aozora::SWF
{
    struct FavoriteSnapshotEntry
    {
        std::uint32_t formID{ 0 };
        std::uint64_t instanceKey{ 0 };
        std::uint32_t hotkeySlot{ 0xFFFFFFFFu };
        std::string name;
        std::string type;
        std::uint32_t count{ 0 };
        bool equipped{ false };
        std::string iconLibrary;
        std::string iconClass;
        std::string iconCategory;
        std::uint32_t useCount{ 0 };
    };

    void InitializeStore();
    void RefreshStore();
    std::vector<FavoriteSnapshotEntry> Snapshot();
    void RegisterFavoriteChangeSink();
    void ClearVanillaFavorites();

    bool AddFavorite(std::uint32_t a_formID, std::uint64_t a_instanceKey = 0,
        std::uint32_t a_hotkeySlot = 0xFFFFFFFFu);
    // Pip-Boy Q/legacy bridge. The stack index is resolved before the
    // instance key is calculated so same-form, differently-modded weapons
    // remain distinct and never fall back to the first stack.
    bool ToggleFavoriteForStack(std::uint32_t a_formID, std::uint32_t a_stackID);
    bool ToggleFavoriteForPipboyIndex(std::uint32_t a_selectedIndex);
    bool RemoveFavorite(std::uint32_t a_formID, std::uint64_t a_instanceKey = 0);
    bool AssignHotkey(std::uint32_t a_formID, std::uint64_t a_instanceKey,
        std::uint32_t a_hotkeySlot);
    bool ActivateFavorite(std::uint32_t a_formID, std::uint64_t a_instanceKey = 0);
    bool HasHotkeySlot(std::uint32_t a_hotkeySlot);
    bool ActivateHotkeySlot(std::uint32_t a_hotkeySlot);

    void Save(const F4SE::SerializationInterface* a_interface);
    void Load(const F4SE::SerializationInterface* a_interface);
    void Revert(const F4SE::SerializationInterface* a_interface);
}

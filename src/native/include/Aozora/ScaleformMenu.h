#pragma once

#include <string_view>
#include <vector>

#include "F4SE/F4SE.h"
#include "RE/G/GameMenuBase.h"
#include "RE/S/SWFToCodeFunctionHandler.h"
#include "Aozora/FavoriteStore.h"

namespace Aozora::SWF
{
    // Resolve the row selected by the real Pip-Boy inventory to the exact
    // inventory identity for the existing Q/legacy bridge.
    bool ResolvePipboySelectionByIndex(std::uint32_t a_selectedIndex,
        std::uint32_t& a_formID, std::uint32_t& a_stackID);

    class FavoritesMenu final : public RE::GameMenuBase
    {
    public:
        static constexpr std::string_view MENU_NAME = "AozoraFavoritesMenu";
        static constexpr std::string_view MOVIE_PATH = "Interface/AozoraFavoritesMenu.swf";

        FavoritesMenu();
        ~FavoritesMenu() override;

        FavoritesMenu(const FavoritesMenu&) = delete;
        FavoritesMenu(FavoritesMenu&&) = delete;
        FavoritesMenu& operator=(const FavoritesMenu&) = delete;
        FavoritesMenu& operator=(FavoritesMenu&&) = delete;

        void Call(const Params& a_params) override;
        void MapCodeObjectFunctions() override;
        void OnAddedToMenuStack() override;
        void OnRemovedFromMenuStack() override;
        bool ShouldHandleEvent(const RE::InputEvent* a_event) override;
        void OnButtonEvent(const RE::ButtonEvent* a_event) override;
        void OnThumbstickEvent(const RE::ThumbstickEvent* a_event) override;

        void PushSnapshot();

        static RE::IMenu* Create(const RE::UIMessage& a_message);
        static void Register();
        static void RegisterInput();
        static void InstallFavoritesInputHook();
        static void WriteLog(std::string_view a_message);
        static void Open();
        static void Close();
        static bool IsOpen();
        static void RefreshSnapshotIfOpen();
        static void HandlePlayerButton(const RE::ButtonEvent* a_event);
        static void HandlePlayerThumbstick(const RE::ThumbstickEvent* a_event);

    private:
        enum class NativeFunction : std::intptr_t
        {
            kInitialize = 0,
            kClose = 1,
            kRequestPause = 2,
            kRequestSnapshot = 3,
            kActivateFavorite = 4,
            kRemoveFavorite = 5,
            kAssignHotkey = 6,
            kSelectFavorite = 7,
            kSelectCategory = 8,
            kWriteLog = 9
        };

        void PushStatus(const char* a_status);
        void InstallDirectCodeObjectFunctions();
        void MoveSelectionNative(int a_delta, std::uint32_t a_focusSource = 1);
        void MoveCategoryNative(int a_delta, std::uint32_t a_focusSource = 1);
        void ActivateSelectionNative();
        void RemoveSelectionNative();
        void AssignSelectionHotkey(std::uint32_t a_slot);
        void SelectCategoryNative(std::uint32_t a_category);
        void SelectIdentityNative(std::uint32_t a_formID, std::uint64_t a_instanceKey);
        std::vector<std::size_t> VisibleEntryIndexes() const;
        void BeginSession();
        void EndSession();
        void RequestPause();

        bool sessionActive_{ false };
        bool movieLoaded_{ false };
        bool timeCaptured_{ false };
        float previousTimeMultiplier_{ 1.0F };
        bool previousFreezeTime_{ false };
        std::vector<FavoriteSnapshotEntry> entries_;
        std::uint32_t categoryIndex_{ 0 };
        std::uint32_t selectedIndex_{ 0 };
        // 0=none, 1=keyboard, 2=gamepad, 3=mouse. The AS3 layer uses this
        // to prevent a synthetic MOUSE_OVER from stealing a keyboard/stick
        // selection immediately after a snapshot rebuild.
        std::uint32_t focusSource_{ 0 };
    };
}

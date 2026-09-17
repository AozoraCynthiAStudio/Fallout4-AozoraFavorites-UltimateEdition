#include "Aozora/Settings.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <utility>

#include "RE/Fallout.h"
#include "RE/H/HUDMenuUtils.h"

namespace Aozora::SWF
{
    namespace
    {
        constexpr std::string_view SETTINGS_PATH = "Data/MCM/Settings/AozoraFavorites.ini";
        constexpr std::array<std::string_view, kMascotSeriesCount> MASCOT_SERIES{
            "Chat", "Snack", "Mechanic", "Explorer", "Groom" };

        std::string Trim(std::string value)
        {
            const auto left = value.find_first_not_of(" \t\r\n");
            if (left == std::string::npos) {
                return {};
            }
            const auto right = value.find_last_not_of(" \t\r\n");
            return value.substr(left, right - left + 1);
        }

        bool ReadSectionValue(std::string_view sectionName, std::string_view keyName,
            std::string& valueOut)
        {
            std::ifstream file(SETTINGS_PATH.data());
            if (!file.is_open()) {
                return false;
            }
            std::string section;
            std::string line;
            while (std::getline(file, line)) {
                const auto comment = line.find(';');
                if (comment != std::string::npos) {
                    line.resize(comment);
                }
                line = Trim(std::move(line));
                if (line.empty()) {
                    continue;
                }
                if (line.front() == '[' && line.back() == ']') {
                    section = Trim(line.substr(1, line.size() - 2));
                    continue;
                }
                if (section != sectionName) {
                    continue;
                }
                const auto equals = line.find('=');
                if (equals == std::string::npos || Trim(line.substr(0, equals)) != keyName) {
                    continue;
                }
                valueOut = Trim(line.substr(equals + 1));
                return true;
            }
            return false;
        }

        int ReadIntSetting(std::string_view key, int fallback, int min, int max)
        {
            std::string value;
            if (!ReadSectionValue("General", key, value) &&
                !ReadSectionValue("Layout", key, value)) {
                return fallback;
            }
            try {
                return std::clamp(std::stoi(value), min, max);
            } catch (...) {
                return fallback;
            }
        }

        float ReadFloatSetting(std::string_view section, std::string_view key,
            float fallback, float min, float max)
        {
            std::string value;
            if (!ReadSectionValue(section, key, value)) {
                return fallback;
            }
            try {
                return std::clamp(std::stof(value), min, max);
            } catch (...) {
                return fallback;
            }
        }

        int ReadLayoutInt(std::string_view section, std::string_view key,
            int fallback, int min, int max)
        {
            std::string value;
            if (!ReadSectionValue(section, key, value)) {
                return fallback;
            }
            try {
                return std::clamp(std::stoi(value), min, max);
            } catch (...) {
                return fallback;
            }
        }

        bool ReadLayoutBool(std::string_view section, std::string_view key, bool fallback)
        {
            return ReadLayoutInt(section, key, fallback ? 1 : 0, 0, 1) != 0;
        }

        MascotLayoutSettings DefaultMascot(float x, float y, float scale)
        {
            return { x, y, scale, 0.0F, 0.96F };
        }

        SkyuiLikeLayout DefaultLayout()
        {
            SkyuiLikeLayout layout{};
            layout.panelX = 36.0F; layout.panelY = 78.0F; layout.panelScale = 1.0F;
            layout.width = 322.0F; layout.height = 560.0F;
            layout.contentX = 0.0F; layout.contentY = 0.0F;
            layout.contentScaleX = 1.0F; layout.contentScaleY = 1.0F;
            layout.titleBlockX = 0.0F; layout.titleBlockY = 0.0F;
            layout.categoryBlockX = 0.0F; layout.categoryBlockY = 0.0F;
            layout.headerBlockX = 0.0F; layout.headerBlockY = 0.0F;
            layout.listBlockX = 0.0F; layout.listBlockY = 0.0F;
            layout.footerBlockX = 0.0F; layout.footerBlockY = 0.0F;
            layout.titleX = 16.0F; layout.titleY = 18.0F; layout.titleFontSize = 29.0F;
            layout.categoryX = 16.0F; layout.categoryY = 67.0F;
            layout.categoryStep = 66.0F; layout.categoryWidth = 54.0F;
            layout.categoryFontSize = 18.0F;
            layout.categoryHitX = 0.0F; layout.categoryHitY = -4.0F;
            layout.categoryHitWidth = 66.0F; layout.categoryHitHeight = 34.0F;
            layout.headerY = 108.0F; layout.headerNameX = 47.0F;
            layout.headerHotkeyX = 210.0F; layout.headerQuantityX = 259.0F;
            layout.headerFontSize = 12.0F;
            layout.listX = 12.0F; layout.listTop = 126.0F;
            layout.rowHeight = 45.0F; layout.rowWidth = 296.0F;
            layout.rowIconX = 13.0F; layout.rowIconY = 10.0F; layout.rowIconScale = 1.0F;
            layout.rowTextBlockY = 0.0F;
            layout.rowNameX = 47.0F; layout.rowNameY = 7.0F;
            layout.rowNameWidth = 150.0F; layout.rowFontSize = 18.0F;
            layout.nameScrollDelay = 0.80F; layout.nameScrollSpeed = 24.0F;
            layout.nameScrollEndPause = 0.90F;
            layout.focusFrameX = 0.0F; layout.focusFrameY = 0.0F;
            layout.focusFrameWidth = 296.0F; layout.focusFrameHeight = 42.0F;
            layout.focusFrameThickness = 1.0F; layout.focusFrameAlpha = 0.98F;
            layout.rowHotkeyX = 210.0F; layout.rowHotkeyY = 7.0F;
            layout.rowHotkeyWidth = 30.0F; layout.rowHotkeyHeight = 29.0F;
            layout.rowHotkeyScale = 1.0F; layout.rowHotkeyTextY = 7.0F;
            layout.rowHotkeyFontSize = 17.0F;
            layout.rowQuantityX = 257.0F; layout.rowQuantityY = 8.0F;
            layout.rowQuantityWidth = 39.0F; layout.rowQuantityFontSize = 16.0F;
            layout.scrollbarX = 307.0F; layout.scrollbarY = 128.0F;
            layout.scrollbarWidth = 4.0F; layout.scrollbarHeight = 360.0F;
            layout.scrollbarThumbMinHeight = 26.0F;
            layout.footerX = 16.0F; layout.footerY = 510.0F;
            layout.footerWidth = 290.0F; layout.footerHeight = 34.0F;
            layout.footerKey1X = 0.0F; layout.footerKey1Y = 0.0F;
            layout.footerKey2X = 151.0F; layout.footerKey2Y = 0.0F;
            layout.footerKey1TextY = 1.0F; layout.footerKey2TextY = 1.0F;
            layout.footerLabel1X = 36.0F; layout.footerLabel1Y = 5.0F;
            layout.footerLabel2X = 187.0F; layout.footerLabel2Y = 5.0F;
            layout.footerDividerX = 129.0F; layout.footerDividerY = 1.0F;
            layout.footerDividerHeight = 28.0F;
            layout.footerKeyGap = 12.0F; layout.footerKeyWidth = 28.0F;
            layout.footerKeyHeight = 34.0F; layout.footerKeyScale = 1.0F;
            layout.footerKeyTextY = 1.0F; layout.footerLabelXOffset = 8.0F;
            layout.footerLabelY = 5.0F; layout.footerBottomLineY = 44.0F;
            layout.footerDetailY = 51.0F;
            layout.footerKeyFontSize = 16.0F; layout.footerLabelFontSize = 15.0F;
            layout.topMicroFontSize = 8.0F; layout.topMicroAlpha = 0.55F;
            layout.footerDetailFontSize = 7.0F; layout.footerDetailAlpha = 0.55F;
            layout.borderAlpha = 0.86F;
            layout.outerHorizontalAlpha = 0.28F; layout.outerVerticalAlpha = 0.42F;
            layout.innerHorizontalAlpha = 0.75F; layout.innerVerticalAlpha = 0.66F;
            layout.frameCornerRadius = 8.0F;
            layout.backgroundAlpha = 0.88F;
            layout.rowAlpha = 0.22F; layout.dividerAlpha = 0.36F;
            layout.scanlineAlpha = 0.045F; layout.silhouetteAlpha = 0.035F;
            layout.silhouetteX = 150.0F; layout.silhouetteY = 230.0F; layout.silhouetteScale = 0.28F;
            layout.mascotBackdropX = 255.0F; layout.mascotBackdropY = 66.0F;
            layout.mascotBackdropScale = 0.70F; layout.mascotBackdropAlpha = 0.14F;
            for (auto& mascot : layout.mascots) {
                mascot = DefaultMascot(230.0F, -42.0F, 0.255F);
            }
            return layout;
        }

        void ReadLayout(std::string_view section, SkyuiLikeLayout& layout)
        {
            const auto read = [&](std::string_view key, float fallback, float min, float max) {
                return ReadFloatSetting(section, key, fallback, min, max);
            };
            layout.panelX = read("PanelX", layout.panelX, -2000.0F, 2000.0F);
            layout.panelY = read("PanelY", layout.panelY, -2000.0F, 2000.0F);
            layout.panelScale = read("PanelScale", layout.panelScale, 0.05F, 3.0F);
            layout.width = read("Width", layout.width, 180.0F, 800.0F);
            layout.height = read("Height", layout.height, 300.0F, 1000.0F);
            layout.contentX = read("ContentX", layout.contentX, -500.0F, 500.0F);
            layout.contentY = read("ContentY", layout.contentY, -500.0F, 500.0F);
            layout.contentScaleX = read("ContentScaleX", layout.contentScaleX, 0.25F, 2.0F);
            layout.contentScaleY = read("ContentScaleY", layout.contentScaleY, 0.25F, 2.0F);
            layout.titleBlockX = read("TitleBlockX", layout.titleBlockX, -1000.0F, 1000.0F);
            layout.titleBlockY = read("TitleBlockY", layout.titleBlockY, -1000.0F, 1000.0F);
            layout.categoryBlockX = read("CategoryBlockX", layout.categoryBlockX, -1000.0F, 1000.0F);
            layout.categoryBlockY = read("CategoryBlockY", layout.categoryBlockY, -1000.0F, 1000.0F);
            layout.headerBlockX = read("HeaderBlockX", layout.headerBlockX, -1000.0F, 1000.0F);
            layout.headerBlockY = read("HeaderBlockY", layout.headerBlockY, -1000.0F, 1000.0F);
            layout.listBlockX = read("ListBlockX", layout.listBlockX, -1000.0F, 1000.0F);
            layout.listBlockY = read("ListBlockY", layout.listBlockY, -1000.0F, 1000.0F);
            layout.footerBlockX = read("FooterBlockX", layout.footerBlockX, -1000.0F, 1000.0F);
            layout.footerBlockY = read("FooterBlockY", layout.footerBlockY, -1000.0F, 1000.0F);
            layout.titleX = read("TitleX", layout.titleX, -500.0F, 1000.0F);
            layout.titleY = read("TitleY", layout.titleY, -500.0F, 1000.0F);
            layout.titleFontSize = read("TitleFontSize", layout.titleFontSize, 8.0F, 64.0F);
            layout.categoryX = read("CategoryX", layout.categoryX, -500.0F, 1000.0F);
            layout.categoryY = read("CategoryY", layout.categoryY, -500.0F, 1000.0F);
            layout.categoryStep = read("CategoryStep", layout.categoryStep, 8.0F, 200.0F);
            layout.categoryWidth = read("CategoryWidth", layout.categoryWidth, 8.0F, 200.0F);
            layout.categoryFontSize = read("CategoryFontSize", layout.categoryFontSize, 8.0F, 48.0F);
            layout.categoryHitX = read("CategoryHitX", layout.categoryHitX, -100.0F, 200.0F);
            layout.categoryHitY = read("CategoryHitY", layout.categoryHitY, -100.0F, 100.0F);
            layout.categoryHitWidth = read("CategoryHitWidth", layout.categoryHitWidth, 8.0F, 240.0F);
            layout.categoryHitHeight = read("CategoryHitHeight", layout.categoryHitHeight, 8.0F, 100.0F);
            layout.headerY = read("HeaderY", layout.headerY, -500.0F, 1000.0F);
            layout.headerNameX = read("HeaderNameX", layout.headerNameX, -500.0F, 1000.0F);
            layout.headerHotkeyX = read("HeaderHotkeyX", layout.headerHotkeyX, -500.0F, 1000.0F);
            layout.headerQuantityX = read("HeaderQuantityX", layout.headerQuantityX, -500.0F, 1000.0F);
            layout.headerFontSize = read("HeaderFontSize", layout.headerFontSize, 6.0F, 40.0F);
            layout.listX = read("ListX", layout.listX, -500.0F, 1000.0F);
            layout.listTop = read("ListTop", layout.listTop, -500.0F, 1200.0F);
            layout.rowHeight = read("RowHeight", layout.rowHeight, 20.0F, 120.0F);
            layout.rowWidth = read("RowWidth", layout.rowWidth, 80.0F, 780.0F);
            layout.rowIconX = read("RowIconX", layout.rowIconX, -500.0F, 1000.0F);
            layout.rowIconY = read("RowIconY", layout.rowIconY, -500.0F, 1000.0F);
            layout.rowIconScale = read("RowIconScale", layout.rowIconScale, 0.05F, 5.0F);
            layout.rowTextBlockY = read("RowTextBlockY", layout.rowTextBlockY, -100.0F, 200.0F);
            layout.rowNameX = read("RowNameX", layout.rowNameX, -500.0F, 1000.0F);
            layout.rowNameY = read("RowNameY", layout.rowNameY, -500.0F, 1000.0F);
            layout.rowNameWidth = read("RowNameWidth", layout.rowNameWidth, 20.0F, 600.0F);
            layout.rowFontSize = read("RowFontSize", layout.rowFontSize, 6.0F, 48.0F);
            layout.nameScrollDelay = read("NameScrollDelay", layout.nameScrollDelay, 0.0F, 10.0F);
            layout.nameScrollSpeed = read("NameScrollSpeed", layout.nameScrollSpeed, 1.0F, 200.0F);
            layout.nameScrollEndPause = read("NameScrollEndPause", layout.nameScrollEndPause, 0.0F, 10.0F);
            layout.focusFrameX = read("FocusFrameX", layout.focusFrameX, -500.0F, 1000.0F);
            layout.focusFrameY = read("FocusFrameY", layout.focusFrameY, -500.0F, 500.0F);
            layout.focusFrameWidth = read("FocusFrameWidth", layout.focusFrameWidth, 20.0F, 780.0F);
            layout.focusFrameHeight = read("FocusFrameHeight", layout.focusFrameHeight, 10.0F, 200.0F);
            layout.focusFrameThickness = read("FocusFrameThickness", layout.focusFrameThickness, 0.5F, 6.0F);
            layout.focusFrameAlpha = read("FocusFrameAlpha", layout.focusFrameAlpha, 0.0F, 1.0F);
            layout.rowHotkeyX = read("RowHotkeyX", layout.rowHotkeyX, -500.0F, 1000.0F);
            layout.rowHotkeyY = read("RowHotkeyY", layout.rowHotkeyY, -500.0F, 1000.0F);
            layout.rowHotkeyWidth = read("RowHotkeyWidth", layout.rowHotkeyWidth, 8.0F, 200.0F);
            layout.rowHotkeyHeight = read("RowHotkeyHeight", layout.rowHotkeyHeight, 8.0F, 200.0F);
            layout.rowHotkeyScale = read("RowHotkeyScale", layout.rowHotkeyScale, 0.25F, 3.0F);
            layout.rowHotkeyTextY = read("RowHotkeyTextY", layout.rowHotkeyTextY, -100.0F, 200.0F);
            layout.rowHotkeyFontSize = read("RowHotkeyFontSize", layout.rowHotkeyFontSize, 6.0F, 48.0F);
            layout.rowQuantityX = read("RowQuantityX", layout.rowQuantityX, -500.0F, 1000.0F);
            layout.rowQuantityY = read("RowQuantityY", layout.rowQuantityY, -500.0F, 1000.0F);
            layout.rowQuantityWidth = read("RowQuantityWidth", layout.rowQuantityWidth, 8.0F, 200.0F);
            layout.rowQuantityFontSize = read("RowQuantityFontSize", layout.rowQuantityFontSize, 6.0F, 48.0F);
            layout.scrollbarX = read("ScrollbarX", layout.scrollbarX, -500.0F, 1000.0F);
            layout.scrollbarY = read("ScrollbarY", layout.scrollbarY, -500.0F, 1200.0F);
            layout.scrollbarWidth = read("ScrollbarWidth", layout.scrollbarWidth, 1.0F, 80.0F);
            layout.scrollbarHeight = read("ScrollbarHeight", layout.scrollbarHeight, 20.0F, 1000.0F);
            layout.scrollbarThumbMinHeight = read("ScrollbarThumbMinHeight", layout.scrollbarThumbMinHeight, 4.0F, 500.0F);
            layout.footerX = read("FooterX", layout.footerX, -500.0F, 1000.0F);
            layout.footerY = read("FooterY", layout.footerY, -500.0F, 1200.0F);
            layout.footerWidth = read("FooterWidth", layout.footerWidth, 40.0F, 780.0F);
            layout.footerHeight = read("FooterHeight", layout.footerHeight, 10.0F, 200.0F);
            layout.footerKey1X = read("FooterKey1X", layout.footerKey1X, -500.0F, 1000.0F);
            layout.footerKey1Y = read("FooterKey1Y", layout.footerKey1Y, -500.0F, 500.0F);
            layout.footerKey2X = read("FooterKey2X", layout.footerKey2X, -500.0F, 1000.0F);
            layout.footerKey2Y = read("FooterKey2Y", layout.footerKey2Y, -500.0F, 500.0F);
            layout.footerKey1TextY = read("FooterKey1TextY", layout.footerKey1TextY, -100.0F, 200.0F);
            layout.footerKey2TextY = read("FooterKey2TextY", layout.footerKey2TextY, -100.0F, 200.0F);
            layout.footerLabel1X = read("FooterLabel1X", layout.footerLabel1X, -500.0F, 1000.0F);
            layout.footerLabel1Y = read("FooterLabel1Y", layout.footerLabel1Y, -500.0F, 500.0F);
            layout.footerLabel2X = read("FooterLabel2X", layout.footerLabel2X, -500.0F, 1000.0F);
            layout.footerLabel2Y = read("FooterLabel2Y", layout.footerLabel2Y, -500.0F, 500.0F);
            layout.footerDividerX = read("FooterDividerX", layout.footerDividerX, -500.0F, 1000.0F);
            layout.footerDividerY = read("FooterDividerY", layout.footerDividerY, -500.0F, 500.0F);
            layout.footerDividerHeight = read("FooterDividerHeight", layout.footerDividerHeight, 0.0F, 200.0F);
            layout.footerKeyGap = read("FooterKeyGap", layout.footerKeyGap, 0.0F, 100.0F);
            layout.footerKeyWidth = read("FooterKeyWidth", layout.footerKeyWidth, 8.0F, 200.0F);
            layout.footerKeyHeight = read("FooterKeyHeight", layout.footerKeyHeight, 8.0F, 200.0F);
            layout.footerKeyScale = read("FooterKeyScale", layout.footerKeyScale, 0.25F, 3.0F);
            layout.footerKeyTextY = read("FooterKeyTextY", layout.footerKeyTextY, -100.0F, 200.0F);
            layout.footerLabelXOffset = read("FooterLabelXOffset", layout.footerLabelXOffset, -100.0F, 200.0F);
            layout.footerLabelY = read("FooterLabelY", layout.footerLabelY, -100.0F, 200.0F);
            layout.footerBottomLineY = read("FooterBottomLineY", layout.footerHeight + 10.0F, -100.0F, 300.0F);
            layout.footerDetailY = read("FooterDetailY", layout.footerDetailY, -100.0F, 300.0F);
            layout.footerKeyFontSize = read("FooterKeyFontSize", layout.footerKeyFontSize, 6.0F, 48.0F);
            layout.footerLabelFontSize = read("FooterLabelFontSize", layout.footerLabelFontSize, 6.0F, 48.0F);
            layout.topMicroFontSize = read("TopMicroFontSize", layout.topMicroFontSize, 4.0F, 40.0F);
            layout.topMicroAlpha = read("TopMicroAlpha", layout.topMicroAlpha, 0.0F, 1.0F);
            layout.footerDetailFontSize = read("FooterDetailFontSize", layout.footerDetailFontSize, 4.0F, 40.0F);
            layout.footerDetailAlpha = read("FooterDetailAlpha", layout.footerDetailAlpha, 0.0F, 1.0F);
            layout.borderAlpha = read("BorderAlpha", layout.borderAlpha, 0.0F, 1.0F);
            layout.outerHorizontalAlpha = read("OuterHorizontalAlpha", layout.outerHorizontalAlpha, 0.0F, 1.0F);
            layout.outerVerticalAlpha = read("OuterVerticalAlpha", layout.outerVerticalAlpha, 0.0F, 1.0F);
            layout.innerHorizontalAlpha = read("InnerHorizontalAlpha", layout.innerHorizontalAlpha, 0.0F, 1.0F);
            layout.innerVerticalAlpha = read("InnerVerticalAlpha", layout.innerVerticalAlpha, 0.0F, 1.0F);
            layout.frameCornerRadius = read("FrameCornerRadius", layout.frameCornerRadius, 0.0F, 100.0F);
            layout.backgroundAlpha = read("BackgroundAlpha", layout.backgroundAlpha, 0.0F, 1.0F);
            layout.rowAlpha = read("RowAlpha", layout.rowAlpha, 0.0F, 1.0F);
            layout.dividerAlpha = read("DividerAlpha", layout.dividerAlpha, 0.0F, 1.0F);
            layout.scanlineAlpha = read("ScanlineAlpha", layout.scanlineAlpha, 0.0F, 1.0F);
            layout.silhouetteAlpha = read("SilhouetteAlpha", layout.silhouetteAlpha, 0.0F, 1.0F);
            layout.silhouetteX = read("SilhouetteX", layout.silhouetteX, -2000.0F, 2000.0F);
            layout.silhouetteY = read("SilhouetteY", layout.silhouetteY, -2000.0F, 2000.0F);
            layout.silhouetteScale = read("SilhouetteScale", layout.silhouetteScale, 0.02F, 3.0F);
            layout.mascotBackdropX = read("MascotBackdropX", layout.mascotBackdropX, -2000.0F, 2000.0F);
            layout.mascotBackdropY = read("MascotBackdropY", layout.mascotBackdropY, -2000.0F, 2000.0F);
            layout.mascotBackdropScale = read("MascotBackdropScale", layout.mascotBackdropScale, 0.02F, 5.0F);
            layout.mascotBackdropAlpha = read("MascotBackdropAlpha", layout.mascotBackdropAlpha, 0.0F, 1.0F);
            for (std::size_t i = 0; i < MASCOT_SERIES.size(); ++i) {
                const auto prefix = std::string("Mascot") + std::string(MASCOT_SERIES[i]);
                auto& mascot = layout.mascots[i];
                mascot.x = ReadFloatSetting(section, prefix + "X", mascot.x, -2000.0F, 2000.0F);
                mascot.y = ReadFloatSetting(section, prefix + "Y", mascot.y, -2000.0F, 2000.0F);
                mascot.scale = ReadFloatSetting(section, prefix + "Scale", mascot.scale, 0.02F, 3.0F);
                mascot.rotation = ReadFloatSetting(section, prefix + "Rotation", mascot.rotation, -45.0F, 45.0F);
                mascot.alpha = ReadFloatSetting(section, prefix + "Alpha", mascot.alpha, 0.0F, 1.0F);
            }
        }

        int DPadAction(RE::BS_BUTTON_CODE code)
        {
            auto readDPad = [](std::string_view key) {
                return ReadIntSetting(key, 1, 0, 1) == 1 ? 2 : 0;
            };
            switch (code) {
            case RE::BS_BUTTON_CODE::kDPAD_Up: return readDPad("iDPadUpAction");
            case RE::BS_BUTTON_CODE::kDPAD_Down: return readDPad("iDPadDownAction");
            case RE::BS_BUTTON_CODE::kDPAD_Left: return readDPad("iDPadLeftAction");
            case RE::BS_BUTTON_CODE::kDPAD_Right: return readDPad("iDPadRightAction");
            default: return 1;
            }
        }
    }

    float ReadSlowMotionScale()
    {
        return ReadFloatSetting("General", "fSlowMotionScale", 0.0F, 0.0F, 1.0F);
    }

    int ReadAllSortMode() { return ReadIntSetting("iAllSortMode", 0, 0, 1); }
    int ReadIconMode() { return ReadIntSetting("iIconMode", 2, 0, 2); }
    bool ReadShowItemInnerName() { return ReadIntSetting("bShowItemInnerName", 0, 0, 1) != 0; }
    bool ReadMascotEnabled() { return ReadIntSetting("bMascotEnabled", 1, 0, 1) != 0; }

    ThemeSettings ReadThemeSettings()
    {
        ThemeSettings theme{};
        theme.mode = ReadIntSetting("iThemeColorMode", 0, 0, 1);
        theme.preset = ReadIntSetting("iThemeColorPreset", 0, 0, 5);
        if (theme.mode == 0) {
            const auto hud = RE::HUDMenuUtils::GetGameplayHUDColor();
            theme.r = std::clamp(hud.r, 0.0F, 1.0F);
            theme.g = std::clamp(hud.g, 0.0F, 1.0F);
            theme.b = std::clamp(hud.b, 0.0F, 1.0F);
            if (theme.r + theme.g + theme.b < 0.05F) {
                theme.r = 0.40F; theme.g = 1.0F; theme.b = 0.70F;
            }
        } else {
            constexpr std::array<std::array<float, 3>, 6> presets{{
                { 0.40F, 1.00F, 0.70F },
                { 1.00F, 0.78F, 0.28F },
                { 0.38F, 0.88F, 1.00F },
                { 1.00F, 0.42F, 0.30F },
                { 0.82F, 0.58F, 1.00F },
                { 0.92F, 0.96F, 1.00F }
            }};
            const auto& color = presets[static_cast<std::size_t>(theme.preset)];
            theme.r = color[0]; theme.g = color[1]; theme.b = color[2];
        }
        return theme;
    }

    LayoutSettings ReadLayoutSettings()
    {
        LayoutSettings settings{
            ReadLayoutBool("Layout", "bLayoutEditorMode", false),
            ReadFloatSetting("Layout", "fLayoutRefreshSeconds", 0.50F, 0.10F, 10.0F),
            DefaultLayout()
        };
        ReadLayout("SkyuiLike16x9", settings.skyuiLike16x9);
        return settings;
    }

    int DPadActionForInput(std::string_view userEvent, RE::BS_BUTTON_CODE code)
    {
        if (userEvent == "QuickkeyUp") return ReadIntSetting("iDPadUpAction", 1, 0, 1) == 1 ? 2 : 0;
        if (userEvent == "QuickkeyDown") return ReadIntSetting("iDPadDownAction", 1, 0, 1) == 1 ? 2 : 0;
        if (userEvent == "QuickkeyLeft") return ReadIntSetting("iDPadLeftAction", 1, 0, 1) == 1 ? 2 : 0;
        if (userEvent == "QuickkeyRight") return ReadIntSetting("iDPadRightAction", 1, 0, 1) == 1 ? 2 : 0;
        return DPadAction(code);
    }
}

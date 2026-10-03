#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "RE/B/BS_BUTTON_CODE.h"

namespace Aozora::SWF
{
    enum class DPadInputAction : std::uint8_t
    {
        VanillaFavorites = 0,
        AozoraFavorites = 1,
    };

    enum class LogLevel : std::uint8_t
    {
        Error = 0,
        Warning = 1,
        Info = 2,
        Debug = 3
    };

    inline constexpr std::size_t kMascotSeriesCount = 5;

    struct MascotLayoutSettings
    {
        float x;
        float y;
        float scale;
        float rotation;
        float alpha;
    };

    struct ThemeSettings
    {
        int mode;
        int preset;
        float r;
        float g;
        float b;
    };

    struct SkyuiLikeLayout
    {
        float panelX;
        float panelY;
        float panelScale;
        float width;
        float height;
        float contentX;
        float contentY;
        float contentScaleX;
        float contentScaleY;
        float titleBlockX;
        float titleBlockY;
        float categoryBlockX;
        float categoryBlockY;
        float headerBlockX;
        float headerBlockY;
        float listBlockX;
        float listBlockY;
        float footerBlockX;
        float footerBlockY;
        float titleX;
        float titleY;
        float titleFontSize;
        float categoryX;
        float categoryY;
        float categoryStep;
        float categoryWidth;
        float categoryFontSize;
        float categoryHitX;
        float categoryHitY;
        float categoryHitWidth;
        float categoryHitHeight;
        float headerY;
        float headerNameX;
        float headerHotkeyX;
        float headerQuantityX;
        float headerFontSize;
        float listX;
        float listTop;
        float rowHeight;
        float rowWidth;
        float rowIconX;
        float rowIconY;
        float rowIconScale;
        float rowTextBlockY;
        float rowNameX;
        float rowNameY;
        float rowNameWidth;
        float rowFontSize;
        float nameScrollDelay;
        float nameScrollSpeed;
        float nameScrollEndPause;
        float focusFrameX;
        float focusFrameY;
        float focusFrameWidth;
        float focusFrameHeight;
        float focusFrameThickness;
        float focusFrameAlpha;
        float rowHotkeyX;
        float rowHotkeyY;
        float rowHotkeyWidth;
        float rowHotkeyHeight;
        float rowHotkeyScale;
        float rowHotkeyTextY;
        float rowHotkeyFontSize;
        float rowQuantityX;
        float rowQuantityY;
        float rowQuantityWidth;
        float rowQuantityFontSize;
        float scrollbarX;
        float scrollbarY;
        float scrollbarWidth;
        float scrollbarHeight;
        float scrollbarThumbMinHeight;
        float footerX;
        float footerY;
        float footerWidth;
        float footerHeight;
        float footerKey1X;
        float footerKey1Y;
        float footerKey2X;
        float footerKey2Y;
        float footerKey1TextY;
        float footerKey2TextY;
        float footerLabel1X;
        float footerLabel1Y;
        float footerLabel2X;
        float footerLabel2Y;
        float footerDividerX;
        float footerDividerY;
        float footerDividerHeight;
        float footerKeyGap;
        float footerKeyWidth;
        float footerKeyHeight;
        float footerKeyScale;
        float footerKeyTextY;
        float footerLabelXOffset;
        float footerLabelY;
        float footerBottomLineY;
        float footerDetailY;
        float footerKeyFontSize;
        float footerLabelFontSize;
        float topMicroFontSize;
        float topMicroAlpha;
        float footerDetailFontSize;
        float footerDetailAlpha;
        float borderAlpha;
        float outerHorizontalAlpha;
        float outerVerticalAlpha;
        float innerHorizontalAlpha;
        float innerVerticalAlpha;
        float frameCornerRadius;
        float backgroundAlpha;
        float rowAlpha;
        float dividerAlpha;
        float scanlineAlpha;
        float silhouetteAlpha;
        float silhouetteX;
        float silhouetteY;
        float silhouetteScale;
        float mascotBackdropX;
        float mascotBackdropY;
        float mascotBackdropScale;
        float mascotBackdropAlpha;
        std::array<MascotLayoutSettings, kMascotSeriesCount> mascots;
    };

    struct LayoutSettings
    {
        bool editorMode;
        float refreshSeconds;
        SkyuiLikeLayout skyuiLike16x9;
    };

    float ReadSlowMotionScale();
    int ReadLogLevel();
    LogLevel ClassifyLogLevel(std::string_view a_message);
    int ReadAllSortMode();
    int ReadIconMode();
    bool ReadShowItemInnerName();
    bool ReadMascotEnabled();
    ThemeSettings ReadThemeSettings();
    LayoutSettings ReadLayoutSettings();
    DPadInputAction DPadActionForInput(std::string_view a_userEvent, RE::BS_BUTTON_CODE a_code);
    void MigrateDPadSettings();
}

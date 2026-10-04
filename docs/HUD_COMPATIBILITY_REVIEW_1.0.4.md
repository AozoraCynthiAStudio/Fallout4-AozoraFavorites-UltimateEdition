# 1.0.4 HUD compatibility source review

Reviewed 2026-10-04. Evidence is source behavior, not in-game acceptance.

- Fallout4WheelMenu: OpenMenu checks Utility.IsInMenuMode(); RegisterMenu uses MenuFlags=12 (UsesCursor|UsesMenuContext). A visible custom menu alone is not the criterion. https://github.com/asvetliakov/Fallout4WheelMenu/blob/master/fallout/Scripts/Source/User/WheelMenuWidget.psc
- FO4_Overlays: OverlayMenu uses only FlagDoNotPreventGameSave (0x800), no HUD name or AlwaysOpen flag. https://github.com/Scrivener07/FO4_Overlays/blob/master/Data/Scripts/Source/FO4_Overlays/Fallout/Overlays/Menu.psc
- F4SE CustomMenu copies supplied menuFlags; on gamepad it may remove UsesCursor while retaining the other flags. Checking cursor alone is insufficient. https://github.com/ianpatt/f4se/blob/master/f4se/CustomMenu.cpp
- Floating Damage Redux author credits Nameplates source, but the inspected description exposes no Redux source link or menu constructor/flags. Do not claim its exact flags or confirmed compatibility. https://www.nexusmods.com/fallout4/mods/108173?tab=description

Local review: All shortcut/hotkey routes and FavoritesMenu::Open use the shared gameplay gate. The new overlay classification checks interaction flags first, then treats None and MainGameplay contexts as non-interactive. Other contexts remain blocked; existing HUD/button hint exceptions remain. Global UI/menu-mode/player-input checks remain authoritative. A menu that consumes input through private hooks while exposing no flags/context cannot be identified reliably by this heuristic; runtime logs are needed for such mods. Existing CursorMenu/FaderMenu exceptions retained.

Expected boundaries: OverlayMenu 0x800 + None/MainGameplay passes; render/update-only flags + None/MainGameplay pass; WheelMenu 0xC blocks even if gamepad clears cursor to 0x8; modal/pause/movement flags block; BasicMenuNav/Cursor/VATS/etc. contexts block for third-party menus. HUD naming does not override interaction flags.

Runtime feedback: The user reported on 2026-10-04 that the Floating Damage conflict was resolved with the reviewed build. Other HUDs were not individually tested. Opening stutter was reported subsequently; scoped INI reads and reduced diagnostic work were added. Performance improvement has not been measured in-game.

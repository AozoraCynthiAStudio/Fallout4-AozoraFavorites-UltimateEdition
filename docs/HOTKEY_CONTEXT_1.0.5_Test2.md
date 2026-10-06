# 1.0.5 Test2: gameplay hotkey boundaries

User feedback: Quick loot problem no longer reproduced locally; photo mode now works. These observations concern Test1 and do not prove compatibility with every implementation.

1-to-equals item hotkeys and external favorites opening share a gameplay gate. DialogueMenu open or MenuTopicManager.menuOpen blocks them even if dialogue does not pause the game. Do not use residual dialogue-target handles. Text-entry mode blocks them. Interactive input contexts (menu navigation/cursor, console/book, TFC, VATS, lockpick, workshop, appearance, waiting, level-up, quick container, video and Creation Club) block them. Actual free-camera state also blocks them. Existing menu-stack and player-input checks remain.

Normal exploration/combat and ordinary scope context remain allowed, subject to the other guards. Pure visual HUD overlays are still allowed. Native favorites consumer is separately rejected for numeric keys in restricted contexts without mutating the shared event, so XDI can still receive 1/2/3/4. Both custom hotkey activation sites and ShouldHandleEvent use the shared gate. Inside Aozora UI, number keys still assign hotkeys rather than activate gameplay items.

Reference: https://github.com/asvetliakov/Fallout4WheelMenu/blob/master/fallout/Scripts/Source/User/WheelMenuWidget.psc explicitly tests dialogue as well as menu mode and warns that dialogue targets persist. Engine ControlMap separates gameplay Quickkey mappings from interactive contexts: https://gist.github.com/ianpatt/e8066afe5dd9d8627a30 . The chosen allow/block policy is this mod's conservative behavior, not a claim that every vanilla state was runtime traced.

Runtime tests required: all assigned keys 1..= in normal gameplay; XDI choice keys must choose dialogue without equipping; unpaused speaker dialogue; rename/text entry; quick loot; photo mode and TFC; VATS/workshop/lockpick; normal scope; return to normal gameplay; passive Floating Damage HUD. Compiling and archive verification do not establish those runtime results.

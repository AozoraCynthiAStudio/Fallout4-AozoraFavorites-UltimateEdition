# AozoraFavorites Ultimate Edition v1.0.3

## 中文

本次更新主要改善手柄十字键设置与界面焦点兼容性。

- 每个十字键方向现在只有两种选择：**青空菜单**与**原版逻辑**。
- 青空菜单：打开青空收藏；原版逻辑：放行游戏原有收藏处理链，供原版菜单及接管该入口的模组使用。
- 青空菜单打开期间，键盘和手柄按钮由青空 UI 处理，包括设为原版逻辑的十字键方向，避免同一按键同时触发其他菜单。
- 其他有焦点的交互界面打开时，青空快捷入口不响应；普通 HUD 显示不影响使用。
- 修复关闭菜单后偶尔需要多按几次才能重新打开的问题。
- 兼容已有方向设置：青空设置保留，旧的禁用设置改为放行原版逻辑。

安装主体包；需要中文的玩家再安装 CHS Patch，并让中文补丁覆盖主体包。升级后请重启游戏。测试版用户请替换旧测试包。

## English

This update focuses on D-pad configuration and compatibility with other interactive menus.

- Each D-pad direction now has two choices: **Aozora Favorites** and **Original Logic**.
- Aozora Favorites opens this mod's menu. Original Logic passes input through the existing favorites handler for the vanilla menu and mods that replace that entry point.
- While Aozora is open, its UI consumes keyboard and gamepad buttons, including D-pad directions set to Original Logic, to prevent the same input from opening another menu.
- Aozora shortcuts do not respond while another interactive menu has focus. Passive HUD overlays remain supported.
- Fixed intermittent reopening failures that required pressing the shortcut several times.
- Existing Aozora settings are preserved. The previous Disabled option now passes input through Original Logic.

Install the main archive. For Chinese, install the CHS Patch after it and allow the patch to overwrite the main archive. Restart the game after updating. Replace older test packages if installed.

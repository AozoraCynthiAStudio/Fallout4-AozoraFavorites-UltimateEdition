# Aozora Favorites · skyui like v1n

这是新的 UI 基线开发目录。`skyui like` 取代此前的双皮肤设备壳体路线，旧版 `SWF version` 保留为历史基线，不与本目录混用。

## 当前方向

- 侧边窄栏、左重标题、四个紧凑分类：全部 / 武器 / 衣服 / AID。
- 主体由 AS3 动态绘制：面板、标题、分类、列标题、收藏行、快捷键框、数量列、滚动条和底部提示。
- 辐射娘只作为右上角原色挂件；每次菜单会话随机选择一个系列，系列内十个状态只在真实选中项变化时推进。
- 三张单色剪影作为低透明度背景装饰，由主题色染色。
- UI 颜色支持跟随游戏 gameplay HUD color，或从五组预设中选择。
- 布局保留 16:9 / 16:10 两套 INI 参数，并支持打开菜单期间实时重读布局。

## 颜色预设

`iThemeColorMode=0` 跟随游戏 HUD；`1` 使用 `iThemeColorPreset`：

0. Vault Green
1. Amber Terminal
2. Frost Cyan
3. Wasteland Rust
4. Orchid

选中框固定为琥珀黄，辐射娘不染色。

## 构建

```powershell
& .\tools\build-skyui-like-assets.ps1
& .\tools\build-swf.ps1 -FlexSdk 'Y:\Workspace\FO4青空的侧边收藏栏\SWF version\tools\vendor\flex-sdk'
xmake build -y AozoraFavoritesSWF
& .\tools\package-skyui-like-v1n.ps1
```

构建成功、包内容正确和游戏内行为必须分别记录。当前目录仅代表开发基线，尚未声称完成游戏内验证或 MO2 部署。

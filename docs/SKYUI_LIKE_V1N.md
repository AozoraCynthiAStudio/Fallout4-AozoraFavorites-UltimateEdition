# AozoraFavorites Ultimate Edition 实现说明

## 视觉边界

本基线不包含旧设备外壳、实体按键区、双皮肤选择、数量气泡、投影镜头、设备火焰或其他与收藏列表无关的信息。所有功能区都服务于收藏菜单：分类、列表、选择、滚动、使用、取消收藏和快捷键。

## 主题协议

Native 在每次快照中传递 `theme={ mode, preset, r, g, b }`。当模式为 HUD 时，颜色来自运行时 `RE::HUDMenuUtils::GetGameplayHUDColor()`；当模式为预设时，Native 传递固定预设颜色。AS3 只使用该颜色绘制主题层和剪影层，选中态使用独立的固定琥珀黄，辐射娘贴图保持原色。

## 布局协议

Native 只传递当前比例的 `layout.current`。正式版默认关闭实时布局编辑；布局编辑只供开发调试。

## 焦点和输入

列表行仍使用完整 Snapshot 刷新装备、收藏、分类和热键状态。当前可见窗口内的单纯选择移动走 AS3 `UpdateSelectionOnly`，仅更新行状态和操作提示；需要滚动列表或切换键盘/手柄提示时才回退完整 Snapshot。Gameplay 下 D-Pad 无操作项也会被消费，避免打开 vanilla FavoritesMenu；Pip-Boy、Terminal、Dialogue、Workshop 等被阻止上下文保持原有输入路由。

## 资源协议

运行时使用 `Textures/Interface/AozoraFavorites/` 下的 50 张 `skyui_like_look_*.dds` 和 3 张 `skyui_like_silhouette_*.dds`。Mascot PNG 原图是 1024×1536 RGBA，DDS 保持该尺寸并使用 DXT5/BC3、11 级 mip；其余剪影资产的格式和尺寸不变。PNG 原图只放在 `assets/source`，不嵌入 SWF；F4SE `MountImage` / `img://` 负责纹理桥接。`build-skyui-like-assets.ps1 -MascotOnly` 只重建 50 张 mascot DDS。

## 验收顺序

1. AS3 SWF 可编译并加载。
2. Native 可编译，快照字段与 AS3 一致。
3. 53 张运行时纹理存在且命名规范。
4. 游戏内检查 HUD / 五档颜色、长名称、空列表、滚动、键鼠/手柄提示、16:9、16:10 和剪影/辐射娘层级。

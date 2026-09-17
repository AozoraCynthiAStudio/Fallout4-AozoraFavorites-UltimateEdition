# C++ / AS3 通信契约 v1

通信使用 Scaleform GFx 的原生对象、数组、字符串、数字和布尔值。`formID` 以无符号整数传输，`instanceKey` 以 16 位十六进制字符串传输，避免 AS3 `Number` 无法精确保存 64 位整数。

## 原生传给 AS3

菜单完成加载后调用：

```text
root1.Menu_mc.ApplySnapshot(snapshot)
```

`snapshot`：

```text
{
  items: [
    {
      formID: 11259375,
      instanceKey: "0123456789ABCDEF",
      name: "物品名称",
      type: "武器" | "服装" | "药品" | "物品",
      count: 1,
      hotkey: -1,
      equipped: false,
      useCount: 0
    }
  ],
  categoryIndex: 0,
  selectedIndex: 0,
  focusSource: 0 | 1 | 2 | 3,
  mascotEnabled: true,
  theme: { mode: 0 | 1, preset: 0, r: 0.4, g: 1.0, b: 0.7 },
  layout: {
    preset: 1 | 2,
    current: { /* skyui like layout fields */ }
  }
}
```

菜单打开和每次动作后，原生层重新调用 `ApplySnapshot`。分类页签固定为 All、Weapons、Outfits、Aid 四项；其他类型仍可在 All 中显示，但没有单独页签。`focusSource` 为 0=none、1=keyboard、2=gamepad、3=mouse；键盘/手柄导航后的重建不能由静止鼠标触发的合成 `MOUSE_OVER` 抢回焦点，真实 `MOUSE_MOVE` 才能切换到鼠标。选择状态由 AS3 保留并按当前列表视口重新定位。

`theme.mode` 为 0=游戏 HUD、1=预设配色；`theme.preset` 为五档预设索引，`r/g/b` 为已解析的 0 到 1 颜色分量。`layout.current` 是当前比例下的一套 skyui like 布局参数，不再包含旧皮肤对象。

## AS3 调用原生菜单代码对象

```text
Initialize()
ActivateFavorite(formID:uint, instanceKey:String)
RemoveFavorite(formID:uint, instanceKey:String)
AssignHotkey(formID:uint, instanceKey:String, slot:uint)
RequestSnapshot()
Close()
RequestPause()
```

`Initialize` 可重复调用，并返回或触发最新快照。动作调用只表达用户意图，AS3 不先假定游戏操作成功。

`Close(reason)` 的首版原因：`back`、`escape`、`system_menu`、`action`、`load_failure`。原生层必须把重复 Close 当作无操作。

## 选择与动作规则

- `ActivateFavorite` 和 `RemoveFavorite` 只提交 `formID + instanceKey`。
- 原生层解析后再次确认该实例仍存在。
- Test06 首版直接执行装备、药品和物品动作；后续会把武器动画与动作队列拆成独立阶段。
- 取消收藏成功后可以保持菜单打开，并由原生层发送新 revision；是否关闭作为后续体验测试项，不写死在协议里。
- 快捷键槽范围固定为 0 到 11；UI 显示字符由 AS3 映射。

## 协议纪律

增加字段向后兼容；改名、删字段或改变语义必须提升 `protocolVersion`。C++ 与 AS3 启动时记录各自协议版本，版本不一致时显示错误状态并保持可关闭，禁止带着不一致协议继续操作物品。

# D-428 —— 下一片选型：剩余 547 条 fallback 的体部 census（2026-11-23）

方法沿用 D-425：把全部 fallback 去掉 frontmatter 后用 `render_mermaid` 重放（临时探针，
已删除），区分「仅配置阻塞」与「体部阻塞」。本文件只记录证据与排序，不改分类口径。

## 1. 结果

- **547 条 fallback 中 119 条「仅配置阻塞」**，428 条体部阻塞。
- 按家族（仅配置阻塞数）：flowchart 67、er-diagram 9、git 8、cynefin 5、state-v2 4、
  xychart 4、mindmap 3、radar 3、requirement 3、tree-view 3、agentflow 2、packet 2、
  venn 2、c4 1、conf-and-directives 1、ishikawa 1、state 1。
- 已关闭家族：pie（24/24 宽 240 绘制）。treemap 余 10 条全是 `classdef_styling`。

## 2. 候选排序（为什么是这些、为什么不是别的）

1. **packet（2 行，推荐第一片）**：`showBits: false`（真隐藏每位位置号）与
   `bitOrder: descending` + `bitsPerRow: 16`（位号方向与分行）。判据是源自身的
   布局语义，终端可**兑现**而不是惰性披露；packet 渲染器已画位号，改动局部。
2. **radar（3 行）**：两条是 `themeVariables`（`radar.axisColor`、`fontSize`/`cScale0`）
   颜色/字号覆盖，一条是 `config.radar.marginTop/axisScaleFactor` 布局覆盖——全部
   是终端无法表达的像素表现，适合「封闭表 + 披露」模式；但需要引入 `theme: base`
   与 `themeVariables` 封闭子集，表面积比 packet 大。
3. **cynefin（5 行）**：3 条 `dagre_layout_tuning`（config/seed/amplitude）与 2 条
   theme/domain 描述，形状混合，需要先分两组。
- **不选**：venn/ishikawa 的 `look: handDrawn`（外来引擎，与既有 43 类同墙）；
  mindmap 3 条是体部解析能力；xychart 的 4 条里 3 条是冻结的 rotation 历史债
  （排除），余 1 条为负值/小数标签；flowchart 67 条落在 exact-bit 222 冻结面周边。

## 3. 口径说明

本 census 是**选择仪器**，不是准入判据：最终每片仍必须用语义判据（源自身状态标记）
成对新增正负例。①绝对 source_fallback 现为 **547**；与 TL 提到的旧 **328** 口径的
差异（roster 分桶重构、222 exact-bit + 67 受保护/冻结的②桶划界、0035–0048 各 RFC
准入）待 TL 裁决后统一。

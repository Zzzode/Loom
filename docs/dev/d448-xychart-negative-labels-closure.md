# D-448 —— xychart 负值/小数数据标签（2 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」。

## 1. 选片理由与判据

- **为什么选它**：D-443..D-447 之后，剩余 A 型（config 阻塞）行里只有
  `showDataLabel` 的两行属于「按实测可小步闭合」的族内渲染特性。
- **根因（实测二分）**：负值柱的紧凑标签路径在
  `own_sample` 判定处用了半开区间 `[min(end, baseline), max(...))`——正值柱的
  尖端行被包含，负值柱的尖端行被排除，于是负值柱的标签永远被判为"非本系列样例"
  而整源回退；另外一格高的柱子（0.75 在 −10..10 的 20 行画布上只有 1 格）会
  触发 `replaced >= span` 保护。探针矩阵确认：正值+负域可画、整数可画、
  任何 `bar ≤ 0` 的 showDataLabel 行都不画、line 不受影响。
- **预期降幅**：2 行（全局 495 → **493**，formal 1464 → **1466**）。
- **判据（语义）**：标签固定在**柱尖**（零参考线两侧对称），一格高柱子的标签
  放到柱尖外侧（正值上方、负值下方），三个值各出现且仅出现一次；窄 40 的
  垂直行本就可画，按规则 34 实测记录。

## 2. 落地内容

- `src/ui/visual/mermaid_charts_impl.cpp`：不再整体拒绝"show_data + 负最小值"；
  `own_sample` 窗口改为按方向包含柱尖；一格高柱子的标签改为外侧放置；
  零值柱（标签与参考线重合）仍整源回退（无已评审放置）。
- 新门禁 `tests/ui/test_mermaid_xychart_negative_labels_d448_cohort.cpp`（3 组：
  账本钉子／每个值恰好出现一次 + 柱体存在／封闭负向：零值柱、域外值、
  outside-bar 与 line 各自成立）；生成器
  `tools/mermaid/generate_xychart_negative_labels_d448_cohort.py`；
  矩阵分支带 rule-38 可达集证明。
- roster：**493 条 / 35 类**（`--emit` 迁移，删 2 增 0，`--self-test` 绿）。

## 3. 验证

- debug 定向：`MermaidXychartNegativeLabelsD448Cohort.*` **3/3 通过**；
  `MermaidCapabilityReport.OfficialMatrix` 通过；矩阵 1652 drawn / 493
  fallback / 48 gaps / integrity 0，formal 1466。
- 全量门与双预设字节门在本批边界统一执行。

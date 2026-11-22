# D-434 —— tree-view 封闭图标映射表（1 行，家族收口）

日期 2026-11-23 · 本地，未 push · 承接 D-433 同族收尾。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：D-433 之后 tree-view 只剩 2 条可达 fallback，其中 1 条只差
  `filenameIcons` / `extensionIcons` 两个封闭标量映射；另一条卡在共享派发层的
  `---      ` 围栏（不可本地修复）。这是当前最后一条可本地准入的 fallback。
- **预期降幅**：1 行（tree-view 总 fallback 4 → 3；该行此前归 `icon_font_glyphs`
  桶（29 → 28），全局 534 → 533，formal `drawn_at_240` 1425 → 1426）。
- **判据（语义，D-412）**：终端不画图标，映射表的每个条目都惰性，逐条披露
  （`config.treeView.filenameIcons[README.md]=fa:bell -> icon mapping; not applied`
  等），画面文字与 `config` 剥离孪生逐行相等。

## 2. 落地内容（同文件 `mermaid_treeview_impl.cpp`）

- 命名空间文法扩展两个映射：`config.treeView.filenameIcons` /
  `config.treeView.extensionIcons`，每表 ≤ 16 条、键唯一；条目键允许 `.`
  （`README.md`、`.ts`），值允许 `none` 或 `pack[:name]` 小写词，可带引号对
  （`'fa:bell'` 先去引号再校验）。
- 披露顺序固定：标量叶（showIcons → defaultIconPack → rowIndent →
  lineThickness）→ filenameIcons 条目（源序）→ extensionIcons 条目（源序）→
  themeVariables 叶。
- 修正两处真实缺陷（本片发现）：
  1. 映射值在去引号**之前**做词法校验，导致 `'fa:bell'` 被拒；
  2. `leaves` 计数不包含映射条目，导致「只有映射」的 config 被误判为空配置；
     同时严格恢复「已知父级下的未知叶必须拒绝」的守卫（此前被放宽成静默接受）。

## 3. 门与账本

- 账本 `tools/mermaid/treeview-icons-d434-cohort.json`（**1 行**）+ 生成器
  `generate_treeview_icons_d434_cohort.py --check`，digest `8811e7d5…`、
  manifest sha `c966a725…`，行内含 `icon_maps` 期望。
- 新门 `tests/ui/test_mermaid_treeview_icons_d434_cohort.cpp` 3 项：账本钉；
  1 行「绘制 + 孪生相等 + 七行脚注逐字」；映射正例（单表、`none`、`typescript`、
  两表合成的固定顺序）与负例（大写 pack、空值、`fa:`、`fa:bell:x`、键含空格、
  重复条目、条目下再嵌套、重复映射表、映射表挂在 themeVariables、表外叶、
  体部 classDef/:::/init）。
- 矩阵：`treeview_icons_d434_cohort_map()` 分支与 D-433 分支并列，规则 38
  reach proof。

## 4. 既有断言迁移（本片足迹）

- **D-433 门**：嵌套映射负例改为表外叶 `config.treeView.icons: true`
  （映射条目归 D-434 拥有），其余不变。
- **RFC 0031 tree-view 门**：`frontmatter_gated` 的成员集合改为 D-433 ∪ D-434
  两个账本（3 行），其余 2 行仍断言回退。

## 5. 验证（迭代口径）

- 定向：`ctest --preset debug -R 'Treeview'` = **15/15**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-treeview-d434.log{,.exit}`）；日志中逐字出现
  `MermaidTreeviewIconsD434Cohort.LeavesAreInertAgainstConfigFreeTwins`、
  `ClosedIconMapNegativesStayWholeSource`、`GeneratorIsReproducible`、
  `MermaidTreeviewNamespaceD433Cohort.ClosedNamespaceNegativesStayWholeSource`、
  `MermaidTreeviewCohort.GatedTreeviewsStayWholeSource`，均为 `Passed`。
  release 预设同一门 3/3。
- 矩阵（debug，直接跑 `test_mermaid_official`）：64/64 通过，`drawn_at_240`
  **1426**、error 0、`mandatory_gaps` 48、`integrity_errors` 空、reach proof 通过
  （`/tmp/loom-logs/matrix-d434.log`）。
- roster `--emit`：**533 / 36 类**；`icon_font_glyphs` 29 → 28，
  `family_residual:tree-view` 维持 2；`--self-test` / `--matrix` 绿。
- 三数并列：①绝对 source_fallback **533** ②超出认证 profile/冻结权威 **289**
  ③本轮净减少 **1**。

## 6. 未验证 / 剩余风险

- tree-view 剩 3 条：1 条 `:::class` 体部行（`classdef_styling`）、1 条空 `icon()` 体部行、
  1 条 `---      ` 围栏行；后两条与 requirement 的缩进 frontmatter 行都卡在
  共享 classify 探测（`src/ui/visual/mermaid_impl.cpp`）：前端探测只在**恰好**
  `---` 的行闭合 frontmatter。要让这些行准入必须改共享派发，属于需要 TL 授权的
  共享改动，本片未动。
- 映射表条目上限 16 未在语料中出现；若扩展需重审。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

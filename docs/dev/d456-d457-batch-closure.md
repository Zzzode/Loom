# 批边界收口记录 —— D-456…D-457（2026-11-24，第五轮）

本文件是「批目标 fallback ≤ 450」的收口记录。全量 ctest 与双预设字节门只在
批边界跑一次，日志落盘 `/tmp/loom-logs/d45*-*`。

本批**达到** ≤450：**448**。

## 1. 本轮净变化与切片

| 片 | 行数 | 说明 | 提交 |
|---|---|---|---|
| D-456 | 9 | usecase 主题信封（`config.theme` / `config.look` 惰性披露）+ 引号 `systemBoundary` 标题 + `json <id>@{...}` 块 | `4dd23f1a` |
| D-457 | 3 | quadrant 点样式（`classDef` / `:::class` / 行内点样式）与 tree-view `:::class` 节点注解（校验、丢弃、披露） | `0e7aec4a` |

| 项 | 批前（D-455 后） | 批后 |
|---|---|---|
| source_fallback（wide 240） | 460 | **448** |
| drawn | 1685 | **1697** |
| formal `drawn_at_240` | 1499 | **1511** |
| roster | 460 / 30 类 | **448 / 30 类** |

## 2. 行级归属（哪几行从 fallback 转 drawn）

- **D-456（9）**: `usecase/nightly-check/themed-elements-redux-color-theme`、
  `usecase/nightly-check/themed-elements-redux-dark-color-theme`、
  `usecase/pr-check/palette-rotation-redux-color-theme`、
  `usecase/pr-check/palette-rotation-redux-dark-color-theme`、
  `usecase/pr-check/themed-elements-{base,dark,default,forest,neutral}-theme`。
- **D-457（3）**: `quadrant-chart/pr-check/it-should-render-data-points-with-
  styles-classes`、`quadrant-chart/pr-check/it-should-render-data-points-with-
  styles`、`tree-view/pr-check/should-render-class-annotations-for-highlighting`。

两次漂移门在重生成前均只报告这批行（`live 451 != pinned 460 (removed=9)`、
`live 448 != pinned 451 (removed=3)`），无新增或改类。

## 3. 每片测例名（均核到用例名）

- `MermaidUsecaseThemedD456Cohort.LedgerSchemaReviewsAndPins` ✅
- `MermaidUsecaseThemedD456Cohort.LeavesAreInertAgainstFrontmatterFreeTwins` ✅
- `MermaidUsecaseThemedD456Cohort.ClosedNegatives` ✅
- `MermaidStyleD457Cohort.LedgerSchemaReviewsAndPins` ✅
- `MermaidStyleD457Cohort.StylingIsInertAgainstStrippedTwins` ✅
- `MermaidStyleD457Cohort.ClosedNegatives` ✅
- 生成器可复现（`--check`）：`MermaidUsecaseThemedD456Cohort.GeneratorIsReproducible`、
  `MermaidStyleD457Cohort.GeneratorIsReproducible` ✅
- 迁移：`MermaidUsecaseCohort.{LedgerSchemaReviewCountsAndManifest,
  GatedUsecasesStayWholeSource, VisualGateIsKeyBased, ...}`（frontmatter 12→3，
  新增 `d456_admitted` 桶）、`MermaidUsecaseAccessibilityD447Cohort.
  ClosedNamespaceAndAccNegatives`（三条负例按 D-456 替换）、
  `MermaidQuadrantCohort.{LedgerSchemaReviewCountsAndManifest,
  GatedQuadrantsStayWholeSource, VisualGateIsKeyBased, ...}`
  （`body_style_gated` 2→0，新增 `d457_admitted`）、
  `MermaidTreeviewCohort.{LedgerSchemaReviewCountsAndManifest,
  GatedTreeviewsStayWholeSource, VisualGateIsKeyBased, ...}`
  （2→1，新增 `d457_admitted`）、`MermaidTreeviewNamespaceD433Cohort` /
  `MermaidTreeviewIconsD434Cohort` 的 `:::` 负例替换。

## 4. 全量 ctest（release 预设，批边界）

- `cmake --build --preset release -j8`：成功（`build=0`），日志
  `/tmp/loom-logs/d457-release-build.log`。
- 全量：`ctest --test-dir build/release -j8` → **退出码 0**，收集 4818、
  **4816 项 100% 通过**（2 项 Disabled `Not Run`；25 个环境门内 `Skipped`
  子项，均为既有平台/环境跳过，无失败），`Total Test time (real) = 119.40 s`，
  日志 `/tmp/loom-logs/d457-release-ctest.log`。
- 定向（debug）：
  - `ctest -L mermaid` → **1627/1627 通过**（D-456 后，日志
    `/tmp/loom-logs/d456-mermaid-suite.log`）；
  - D-457 后 `ctest -L mermaid` → **1631/1631 通过、退出码 0**
    （`100% tests passed out of 1631`，507.33 s），日志
    `/tmp/loom-logs/d457-mermaid-suite.log`。

## 5. 双预设字节门

- debug 与 release 的 `mermaid-official-matrix.json` sha256 **完全相同**：
  `dfca2ce5e2a24117ed41c1157d3944241bbc2acb6603a633cbd185dd60291ad0`；
  两侧均为 1697 drawn / **448** source_fallback / 0 error / formal 1511 /
  gaps 48 / integrity 0。日志 `/tmp/loom-logs/d457-matrix.log`、
  `/tmp/loom-logs/d457-release-matrix.log`。

## 6. 批边界证据

- release 全量日志中核对到本批新门禁用例名并通过：
  `MermaidUsecaseThemedD456Cohort.*`、`MermaidStyleD457Cohort.*`、
  两侧 `GeneratorIsReproducible`、`MermaidCapabilityReport.OfficialMatrix`。
- roster：`fallback_attribution.py --self-test` 448 行 / 30 类、零未分类、
  分类可复现；`--matrix` 清零漂移。

## 7. 评审

- D-456：独立对抗性评审两轮（REQUEST-CHANGES → APPROVE）。修复：镜像
  `frontmatter_lines` 与渲染器语义对齐（`description` 必需、折叠载荷、空
  命名空间/空 config 门）；`json` 前缀仅在有 `@{` 时进入块解析；补 `look`
  惰性 twin 等价；`systemBoundary` 精确关键字与长度防护；CMake 注释归位；
  `narrow_40` 词表统一。台账记录两轮 agent 身份。
- D-457：三轮（REQUEST-CHANGES ×2 → APPROVE）。修复：点名内 `:` 回归；
  tree-view 注解不得跟在 `icon(...)`/`##` 后；披露预算截断（含前缀差一）；
  台账文案按家族区分；镜像只扫 body；`:::` 标签行回退；UTF-8 码点边界截断 +
  命名常量 + 多字节探针。

## 8. 剩余 448 行的结构与停止条件

| 家族 | fallback / 家族总数 |
|---|---|
| flowchart | 240 / 400（Flow400 冻结契约） |
| swimlanes | 46 / 46（htmlLabels + useMaxWidth 硬墙） |
| class-diagram | 25 / 285 |
| er-diagram | 24 / 206 |
| state-diagram-v2 | 21 / 103 |
| git | 20 / 135（>8 分支容量 / 旋转标签 wont-fix / cherry-pick 父子） |
| sankey | 17 / 18（共享 `classify_mermaid` 裸 `sankey` 派发，D-438 NO-GO 待裁） |
| mindmap | 16 / 33（Work 预算 / icon 字形） |
| xychart | 10 / 48（rotation 3 行为历史债不动；横排行受保护 E6c 观察钉） |
| usecase | 8 / 32（全部为 `classDef`/`style`/`:::class` 体行，其中 2 行另含多行标签） |
| state-diagram | 7 / 42 |
| katex | 4 / 4（无渲染器） |
| tree-view | 1 / 15（组合注解 + 图标字形） |
| 其余（conf/gantt/info/quadrant/requirement/zenuml…） | 各 0–2；quadrant 已闭合 |

继续推进的下一批候选：usecase 完整体语法（typed actor `@{...}` 属性、命名边、
`class`/`style` 语句、多行反引号标签，约 6–8 行）是家族本地可做的最大块；
其余为 Flow400/swimlanes 冻结契约、外部引擎（handDrawn/neo/ELK/dagre/图标）、
静态终端结构性不可达（interactivity/security-level）、或需 TL 裁决的共享派发
（sankey）。

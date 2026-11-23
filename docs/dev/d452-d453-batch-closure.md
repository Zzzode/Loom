# 批边界收口记录 —— D-452…D-453（2026-11-24，第四轮）

本文件是「批目标 fallback ≤ 450」的续推收口。全量 ctest 与双预设字节门只在
批边界跑一次，日志落盘 `/tmp/loom-logs/b6-*`。

本批**未达到** ≤450：在 464 处触发停止条件①（继续推进需要改共享渲染契约 /
新增渲染器 / 容量重基线，均超出家族本地配置准入）。实测证据见 §7。

## 1. 本轮净变化与切片

| 片 | 行数 | 说明 | 提交 |
|---|---|---|---|
| D-452 | 10 | treemap 闭合 `classDef` / `:::` 样式（惰性 + 披露） | `29ca4bba` |
| D-453 | 5 | venn 闭合 `style` 语句，含 flat `text` 子行与 `text id["Label"]` | `29ca4bba` |

| 项 | 批前 | 批后 |
|---|---|---|
| source_fallback（wide 240） | 479 | **464** |
| drawn | 1666 | **1681** |
| formal `drawn_at_240` | 1480 | **1495** |
| roster | 479 / 33 类 | **464 / 32 类** |

三数并列：①**464**（绝对 source_fallback）；②**289**（超出认证 profile /
冻结权威，未变）；③本轮净减少 **15**。

## 2. 每片测例名（均核到用例名）

- `MermaidTreemapClassdefD452Cohort.LedgerSchemaReviewsAndPins` ✅
- `MermaidTreemapClassdefD452Cohort.ClassesAreInertAgainstClassFreeTwins` ✅
- `MermaidTreemapClassdefD452Cohort.ClosedClassNegatives` ✅
- `MermaidVennStyleD453Cohort.LedgerSchemaReviewsAndPins` ✅
- `MermaidVennStyleD453Cohort.StylesAreInertAgainstStyleFreeTwins` ✅
- `MermaidVennStyleD453Cohort.ClosedStyleNegatives` ✅
- 生成器可复现（`--check`）：`MermaidTreemapClassdefD452Cohort` /
  `MermaidVennStyleD453Cohort` 各自绑定的 generator `add_test` ✅
- 迁移：`MermaidTreemapCohort.{LedgerSchemaReviewCountsAndManifest,
  StyledAndConfigTreemapsStayWholeSource, ...}`（`styled_gated` 10→0 并钉死
  空分区）、`MermaidVennCohort.{LedgerSchemaReviewCountsAndManifest,
  GatedVennsStayWholeSource, ...}`（`body_style_gated` 5→0，新增与 D-453
  台账行集的双向成员校验）、`MermaidSmallfam0043Cohort.
  FamilyLedgersPartitionFiveWays`（venn 家族算术并入 D-453 五行）、
  `MermaidTreemapFmtD427Cohort` 与 ishikawa/railroad 台账的零计数钉。

定向回归（debug）：`ctest -R 'MermaidVenn|MermaidTreemap|MermaidSmallfam|
MermaidQuadrant|MermaidC4|GeneratorIsReproducible'` → **90/90 通过**；
`MermaidCapabilityReport` 12 项 → **12/12 通过**（日志
`/tmp/loom-logs/d453-review-fix-matrix.log`）。

## 3. 全量 ctest（release 预设）

- `cmake --preset release` + `cmake --build --preset release -j8`：成功
  （`configure=0`、`build=0`），日志 `/tmp/loom-logs/b6-release-{configure,build}.log`。
- 全量：`ctest --test-dir build/release -j8` → **退出码 0**，收集 4806、
  执行 **4804**、**100% 通过**（0 失败；本批无环境 flake），
  `Total Test time (real) = 121.64 s`，日志
  `/tmp/loom-logs/b6-release-ctest.log`。

## 4. 双预设字节门

- debug 与 release 的 `mermaid-official-matrix.json` sha256 **完全相同**：
  `2f84e632b69a3972e1d52562d0342f5936d12a1af7556e7da1d6eac70f55bb6f`；
  两侧均为 1681 drawn / **464** source_fallback / 0 error / formal 1495 /
  gaps 48 / integrity 0。

## 5. 批边界证据

- release 全量日志中核对到全部新门禁用例名并通过：
  `MermaidVennStyleD453Cohort.{LedgerSchemaReviewsAndPins,
  StylesAreInertAgainstStyleFreeTwins, ClosedStyleNegatives}`（#3651–3653）、
  `MermaidTreemapClassdefD452Cohort.{LedgerSchemaReviewsAndPins,
  ClassesAreInertAgainstClassFreeTwins, ClosedClassNegatives}`（#3654–3656）、
  两侧 `GeneratorIsReproducible`（#3928/#3929）、
  `MermaidCapabilityReport.OfficialMatrix`（#2027，27.11 s）。
- 双预设矩阵 sha256 均为 `2f84e632…`，字节相同（见 §4）。
- 日志：`/tmp/loom-logs/b6-release-{configure,build,ctest}.log`；定向回归
  `/tmp/loom-logs/d453-review-fix-matrix.log`。

## 6. 评审

- 独立对抗性复核（fresh general-purpose agent，D-452/D-453）返回
  **VERDICT: APPROVE**，3 MINOR / 2 NIT；**全部修整**并折叠进 `29ca4bba`：
  1. MINOR 过期注释（treemap/venn 两处仍称 classDef/style 整体门禁）已改；
  2. MINOR venn `text` 括号识别：引号开头的文本不再走 `id["Label"]` 路径
     （`text "a [b] c"` 原样渲染），并补正例；
  3. MINOR venn 重复判定改为按（targets，有序 key=value）归一化键：
     `style A fill:#fff` + `style A fill:#000` 不再被误拒，并补正例；
     C++ 与生成器两侧同步；
  4. NIT 生成器与 C++ 的标识符字符集对齐（venn style target 不再接受 `-`），
     并补负例；
  5. NIT venn 家族门的空分区新增与 D-453 台账行集的双向成员校验。
- 修复后重跑新门禁、家族门禁与 `MermaidCapabilityReport` 全部通过。

## 7. 停止条件①：≤450 不可达的实测证据

剩余 464 行的家族分布（`MermaidCapabilityReport.OfficialMatrix`）：

| 家族 | fallback / 家族总数 |
|---|---|
| flowchart | 240 / 400（Flow400 冻结契约） |
| swimlanes | 46 / 46（htmlLabels+useMaxWidth 硬墙 46/46） |
| class-diagram | 25 / 285 |
| er-diagram | 24 / 206 |
| state-diagram-v2 | 21 / 103 |
| git | 20 / 135（>8 分支容量 / 旋转标签 wont-fix / cherry-pick 合并父子） |
| sankey | 17 / 18（共享 `classify_mermaid` 裸 `sankey` 派发，D-438 NO-GO 待裁） |
| usecase | 17 / 32 |
| mindmap | 16 / 33（Work 预算 / icon 字形） |
| xychart | 10 / 48（rotation 3 行为历史债不动；横排 1 行受保护 E6c 观察钉） |
| state-diagram | 7 / 42 |
| tree-view | 4 / 15 |
| katex | 4 / 4（无渲染器） |
| 其余（conf/gantt/info/quadrant/requirement/timeline/zenuml） | 各 1–2 |

按归属台账的类别（`fallback_attribution.py --self-test`：464 行 / 32 类，
零未分类）：

| 类别 | 行数 | 继续推进所需的改动 |
|---|---|---|
| edge_animation_metadata | 80 | flowchart 共享渲染契约（Flow400 冻结） |
| htmllabels_false | 43 | swimlanes 共享 HTML 标签契约（硬墙 46/46） |
| look_handdrawn / look_neo | 38 / 18 | 外部引擎语义（handDrawn/neo），foreign-engine 政策 |
| securitylevel_directive | 34 | 静态终端结构性不可达（安全等级） |
| subgraph_nesting | 30 | flowchart 共享渲染契约 |
| icon_font_glyphs | 28 | 外部图标字体 |
| classdef_styling | 27 | relations-core 共享样式准入（class/er/state-v2 共用） |
| click_interactive_callback | 18 | 静态终端结构性不可达（交互） |
| family_residual:sankey | 17 | 共享派发谓词（D-438 NO-GO，需 TL 裁决） |
| special_link_markers | 16 | flowchart 共享渲染契约 |
| family_residual:flowchart | 13 | Flow400 冻结契约 |
| family_residual:git | 13 | git 容量/旋转 wont-fix |
| linkstyle_directive | 12 | relations-core 共享样式准入 |
| dagre_layout_tuning / elk_layout_engine | 10 / 10 | 外部布局引擎 |
| family_residual:xychart | 10 | 历史债 + 受保护 E6c 观察 |
| family_residual:mindmap | 7 | Work 预算 / 图标 |
| multiline_label | 7 | 冻结的 body geometry |
| git_cherry_pick | 6 | 合并父子引用（容量探测已定位未解决） |
| family_residual:class-diagram / :state-diagram-v2 | 6 / 6 | relations-core 共享契约 |
| 其余 family_residual 单点 | ≤2 每类 | 新渲染器（info/katex/zenuml）或单点语法 |

**结论**：达到 450 需要再减 14 行，但剩余行中不存在任何 ≥14 行的家族本地
配置准入簇；最小的可行簇（relations-core `classdef_styling` 27 行）就要改
class/er/state-v2 共用的渲染核心，其余簇要么是 flowchart/swimlanes 冻结
共享契约、要么是外部引擎/结构性不可达、要么需要 TL 裁决共享派发谓词。
故按停止条件① 停在 **464**，等待 TL 决策下一里程碑的契约范围。

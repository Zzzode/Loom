# D-400 —— 下一家族选择：按「可达 drawn 增量 / 成本」实测（state 家族）

日期 2026-11-21 · 本地，未 push · 承接 Tech Lead 638 指令（RFC 0037 验收 PASS，
下一家族改判据）与 RFC 0037 收口（`d83b6f48`）。

## 1. 判据与探针方法

TL 明确否决「按原始 fallback 数排序」的口径：fallback 数最大的家族可能恰恰没有干净
切片（flowchart 已因 RFC 0036 被撤回一次）。改用 **可达 drawn 增量 / 实现成本**。

探针（一次性 gtest，运行后删除；日志 `/tmp/reach_probe.log`）：对
`tools/mermaid/fallback_roster.json` 的全部 **754** 条 fallback 行，渲染两次——

1. 原样渲染（按定义为 fallback）；
2. 去掉**整块 frontmatter** 后渲染（`strip=1` 表示 body 本身已能原生绘制）。

再对 `strip=1` 的行做三件事：
- 排除 `tools/mermaid/e6c-mandatory-gap-roster.json` 中的冻结 gap fixture
  （该表含 48 条 flowchart + 25 条 class-diagram，**无 state 行**）；
- 按 frontmatter 的键/值分类（`look`/`theme` 这类闭合表 vs `layout: elk`、`curve`、
  `@{shape}`、`gitGraph.*` 这类几何/语义键）；
- 对「按键看似干净」的最大子集，逐行检查 body 是否藏有能力标记
  （`fa:`、`@{`、`<br>`、`subgraph`、`:::class`、`classDef`、`style`、`click`）。

## 2. 实测结果（strip=1 的行数；括号内为非 gap 数）

| 家族 | fallback | 去 frontmatter 后 body 可绘 | 键分布要点 |
|---|---|---|---|
| flowchart | 240 | 67（49） | 12 `theme`、4 `look+theme`、其余 `layout`/`htmlLabels`/`curve`/`wrappingWidth`/`@{}` |
| git | 77 | 50（50） | 12 `look+theme`；38 为 `gitGraph.*`（parallelCommits 31、showCommitLabel 25、rotateCommitLabel 19、showBranches 19、mainBranchOrder 7） |
| state-diagram-v2 | 70 | 25（25） | **19 行仅 `look`/`theme`**；4 `layout: elk`；2 `state.useMaxWidth` |
| swimlanes | 46 | 0 | 需要泳道渲染器本身 |
| requirement | 28（0037 后） | 26（26） | 剩菜：24→现 0 干净；26 为 elk/命名空间/畸形 |
| class-diagram | 25 | 0 | 全部冻结 `class481` gap |
| er-diagram | 24 | 9（9） | 3 `look`、6 `htmlLabels`（ER 家族另有 107 行冻结权威） |
| timeline | 23 | 16（16） | **16 行全部 `look`/`theme`**（含 dark/forest/neutral） |
| state-diagram (v1) | 23 | 12（12） | **9 行仅 `look`/`theme`**；2 `state.useMaxWidth`；1 折叠描述含制表符 |
| pie | 17 | 13（13） | 5 `theme`；8 为 `pie.donutHole`/`legendPosition`/`textPosition` 等语义键 |
| gantt | 13 | 11（11） | 11 行 `theme`（含 dark/forest/neutral） |
| agentflow | 11 | 11（11） | 9 `theme`；2 `handDrawnSeed+look` |
| 其余（mindmap/treemap/venn/xychart/journey/architecture/c4/conf） | — | ≤6 | 键为 `treemap.valueFormat`、`handDrawnSeed`、`pie.*` 等 |

## 3. flowchart 的据证降级（0 行可达）

按键筛选后 flowchart 有 **16** 行「只带 `look`/`theme`」，逐行查 body 后全部落空：

| body 标记 | 行数 | 例子 |
|---|---|---|
| 嵌套 `subgraph … end` 容器 | 4 | `redux-color-subgraphs/02-should-render-nested-subgraph-containers` |
| `@{ view: collapsed }` 折叠子图 | 4 | `redux-color-subgraphs/03-should-render-collapsed-subgraph-containers` |
| `@{ shape: … }` 形状主题 | 4 | `shape-themes/07-connect-all-shapes-with-each-other` |
| 图标字体 `fa:` + 子图 + `%%` 注释 | 4 | `visual-check/{dark,default,forest,neutral}-theme-legacy-graph` |

即：**flowchart 的 240 条 fallback 中，没有一条的阻塞点只是闭合值表**。这与 D-397 §5
（RFC 0036 撤回时的证伪）一致，但这次是逐行清单，可直接作为降级依据。除非先做
subgraph 嵌套/折叠视图/形状/图标这四类 body 能力，否则 flowchart 不可达。**降级，不硬啃。**

## 4. 排序与选择

按「干净闭合表切片」排序：

| 家族 | 干净行 | 成本评估 |
|---|---|---|
| **state-diagram-v2 + state-diagram** | **28**（19 + 9） | **最低**：`state_source_envelope` 已接受 description/config/title + `logLevel`/`fontFamily`/`securityLevel`，已发布 `config_notices` 页脚，`diagram_title` 已投影——只需再加两个叶子 + 页脚行 + 账本/门 |
| timeline | 16 | 需要全新 envelope 与页脚通道（timeline 无 relations-core 通路） |
| git | 12 | 需要 gitGraph 语义/E2a cohort 的同族工作；`gitGraph.*` 38 行反而是几何语义 |
| gantt | 11 | 需引入 classic 调色板表（dark/forest/neutral），且 gantt 无 envelope |
| agentflow | 9+2 | `handDrawnSeed` 属几何种子，边界要单独裁定 |
| er-diagram | 9 | ER 家族有 107 行冻结权威与 B3 三门，触碰成本高 |

**选择：state 家族（v1 + v2 同一 envelope 与渲染通路），28 行。**
判据同时满足两条：绝对增量最大（28），单位成本最低（信封、页脚、标题投影三项机制
都已存在）。这与 RFC 0037 是同一形态的第二例，可直接复用「闭合表 + 披露页脚 +
twin 门 + 逐阶段账本」的成熟路径。

家族内保留为 capability/延后（不属本 RFC 目标）：
- 4 行 `layout: elk`（另一套布局引擎）；
- 4 行 `state.useMaxWidth`（嵌套命名空间键，需单独的边界决定）；
- 1 行折叠描述里含**制表符**（`<br\t/>`），阻塞点是 description 语法而非 config；
- 其余 55 行（v2 45 + v1 11）body 在去掉 frontmatter 后仍不能绘制，属真实 body 能力。

## 5. 三笔历史债（本次不动）

1. xychart rotation 3 行（需要真实几何）。
2. narrow_40 全回退（宽屏准入不是窄屏承诺）。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/`kB3ReviewStatus`、
   `WRITE_REVIEW_APPROVED`）。

## 6. 复现

- 探针源码（已删除，仅记录方法）：`tests/ui/test_mermaid_family_reach_probe.cpp`
  模式——`render(source, "mermaid"|"text", 240)` 比较、去 frontmatter 再渲染；
  输出 `ROW/<family>/<path>/full/strip/native/keys` 与 `FAMILY` 汇总。
- 日志：`/tmp/reach_probe.log`。本次探针在 release 预设下运行（迭代期用 release）。

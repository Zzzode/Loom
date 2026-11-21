# D-406 —— 下一家族选择：按「可达 drawn 增量 / 成本」实测（agentflow 家族）

日期 2026-11-22 · 本地，未 push · 承接 Tech Lead 指令（RFC 0040 验收 PASS 已 push，
`30bad8a4..a4c5ca2f`，继续下一家族）与 D-402/D-404 的方法。

## 1. 判据

对每个家族只统计**真正可达**的行：把整块 frontmatter 去掉后 body 能在宽 240 原生绘制
（cell-surface 谓词，与官方矩阵一致），且 frontmatter 里除「可闭合、可披露的惰性表
（`look`/`theme`）」之外没有其他阻塞键；再剔除冻结 gap fixture 与 body 能力/
faithfulness 阻塞。最后用「每单位成本」（新增机制面：需要新逻辑的文件/契约数）去除。

## 2. 探针方法与样本（可复现）

- 复用 D-402 的一次性 gtest 探针日志 `/tmp/reach_probe_d402.log`：它对**当时的 726 条
  fallback 行**各渲染两次（原样、去整块 frontmatter），宽 240 高 400，判定谓词是矩阵的
  cell surface（字符+背景；前景色忽略）。fixture 由 manifest 钉死 sha，本切片未改
  agentflow 渲染器，strip 渲染结果不变。
- 本轮把 `journeyfam-0040`（31 行）、`git-0039`（12 行）等已晋升行集从日志中剔除
  （726 − 43 = 683，与当前 roster 完全一致，交集验证 683/683 覆盖），得到**剩余池**：
  261 条 strip 可绘中剩 **218** 条。
- 交叉校验：当前 release 矩阵 `fd852b74…` 的 per-family fallback 计数与日志逐家族一致
  （agentflow 11、flowchart 240、pie 17 等）。
- 结论可信度：日志是临时产物、**不作为门禁证据**；门禁由新 cohort 的 twin/逐行实测承担
  （RFC 0039/0040 同一纪律）。

## 3. 实测结果（剩余 218 条 strip 可绘，按家族）

| 家族 | 剩余 strip 可绘 | 其中「仅惰性 theme/look」 | 其余主要阻塞 |
|---|---|---|---|
| flowchart | 67 | 8（+4 条 look+theme，带 `attr` 标记） | icon font / `@{shape}` / init/acctitle 等 faithfulness；8 条 subgraph 主题行在 2800 行 exact-bit profile 机器上风险最高 |
| git | 38 | 0 | 命名空间 `config.gitGraph.*` 语义键（branch order / parallelCommits / showBranches / rotateCommitLabel） |
| requirement | 26 | 0 | `layout: elk` 等几何语义 |
| **pie** | **13** | **4** | `pie.*` 语义键（highlightSlice / donutHole / legendPosition） |
| **agentflow** | **11** | **9** | 2 条 look + `handDrawnSeed`（seed 决定几何，非惰性） |
| er-diagram | 9 | 0 | ER107 冻结权威 + B3 三门 |
| ishikawa | 6 | 2 | `ishikawa.useMaxWidth`/diagramPadding |
| state-diagram-v2 | 6 | 0 | `layout: elk` / `state.useMaxWidth` / fontFamily |
| treemap | 6 | 0 | `treemap.valueFormat`（数值格式化语义） |
| venn | 5 | 2 | look + handDrawnSeed |
| 其余（cynefin/xychart/mindmap/radar/tree-view/architecture/…） | ≤5 | 0–3 | cynefin 几何参数、xychart 3 条 rotation 历史债、`layout: elk` 等 |

## 4. 成本评估与选择

| 候选 | 可达行 | 机制面 | 风险 |
|---|---|---|---|
| **agentflow** | **9** | **1**：把自有 `af_frontmatter_gated` 从「只准 description」改写为「校验+披露」（与 RFC 0040 同形，TU 本地）；页脚接入 `agentflow_layout` | 低：单文件、单家族、无共享模块/跨家族不变式；body 已实测可绘（同 body × 多主题） |
| flowchart | 8–11 | 1 个但极大：exact-bit profile 机器需新增 4 类 profile 形状并放宽 `look: neo` | 高：RFC 0036 曾因测量被证伪撤回 |
| pie | 4 | 1：扩展既有 `pie_config_frontmatter` 闸门 | 低但增量小 |
| venn / ishikawa | 2 | 1：扩展现有键式 frontmatter 闸门 | 低但增量最小 |
| git 剩余 38 / requirement 26 / state-v2 6 | 0 惰性 | 语义/几何能力 | 不是准入切片 |

**选择：agentflow，9 行**（`theme` ∈ {base, dark, default, forest, neutral,
redux-color, redux-dark-color}，7 个 corpus 值；无 `look`）。理由：单位成本下增量最高
（9 行 / 1 个机制面，且与 git 的 12 行同形）；flowchart 上限相当但风险最高；
pie/venn/ishikawa 增量太小；其余家族的剩余行不是惰性准入问题。

家族结构（实测）：38 条 originals = 27 已绘制 + **9 theme-only 可达** + 2 条
`look`+`handDrawnSeed`（seed 非惰性，保持整源回退）。9 行 = 两种 body ×
{11-container: dark/base/default/forest/neutral} + {triage: dark/default/redux-color/
redux-dark-color}。

## 5. Roster 与门禁副作用（预期，收口时实测）

- roster 683 → **674**；9 行现属 `edge_animation_metadata`（共 91 行），该 detector 是
  共现类（drawn 151 / matched 242，非充分阻塞），晋升后 sufficiency 按实测重算；
  `proven_compact_cohort_candidates` 不含该类别，不受影响。
- 目标数字：agentflow 27/11 → **36/2**；全局 1462/683 → **1471/674**；formal
  `drawn_at_240` 1276 → **1285**；mandatory_gaps 48、integrity 0 不变。
- 9 行均不在受保护 `e6c` mandatory gap roster（该 roster 仅含 class/flowchart）。

## 6. 三笔历史债（本次不动）

1. xychart rotation 3 行（其中 3 条正在本轮候选池，明确不碰）。
2. narrow_40 全回退（宽屏准入不是窄屏承诺）。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/`kB3ReviewStatus`、
   `WRITE_REVIEW_APPROVED`）。

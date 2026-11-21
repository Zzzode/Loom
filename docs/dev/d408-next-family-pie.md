# D-408 —— 下一家族选择：按「可达 drawn 增量 / 成本」实测（pie 家族）

日期 2026-11-22 · 本地，未 push · 承接 Tech Lead 指令（RFC 0041 验收 PASS 已 push，
`30bad8a4..a4c5ca2f` + RFC 0041 两提交，继续下一家族）与 D-406 的方法。

## 1. 判据

对每个家族只统计**真正可达**的行：去掉整块 frontmatter 后 body 能在宽 240 原生绘制
（矩阵 cell-surface 谓词），且除可闭合、可披露的惰性表（`look`/`theme`）外没有别的
阻塞键；剔除冻结 gap fixture 与 body 能力/faithfulness 阻塞；再按「新增机制面」计价。

## 2. 样本（可复现）

复用 D-402 strip 探针日志（fixture 由 manifest 钉死，相关渲染器本轮未变），剔除
RFC 0039/0040/0041 已晋升的 52 行（12 + 31 + 9），得到 674 条剩余池，与当前 release
矩阵 `326fb96f…` 的 per-family fallback 逐家族一致。探针是临时产物，不作为门禁证据；
逐行证明由新 cohort 的 twin/实测承担。

## 3. 实测结果（剩余池中的「仅惰性 theme」子集）

| 家族 | 剩余 fallback | strip 可绘 | 仅惰性 theme/look | 备注 |
|---|---|---|---|---|
| flowchart | 240 | 67 | 15（theme-only）+ 4（look+theme） | **其中 4 条 redux-color-subgraphs 在受保护 `e6c` gap roster（48 条）内**；4 条 legacy-graph 带 icon/init/acctitle 标记（非忠实）；余 7 条仍落在 2800 行 exact-bit profile 机器上，RFC 0036 曾因测量被证伪撤回 |
| git | 65 | 38 | 0 | 命名空间 `gitGraph.*` 语义键 |
| requirement | 28 | 26 | 0 | `layout: elk` 几何 |
| **pie** | **17** | **13** | **5** | 其余 8 条 `pie.*` 语义键（donutHole/highlightSlice/legendPosition/textPosition/logLevel/useMaxWidth），4 条 body 能力阻塞（含 acctitle/accdescr 的 dark/default/forest/neutral 主题行） |
| er-diagram | 24 | 9 | 0 | ER107 冻结权威 + B3 三门 |
| ishikawa | 6 | 6 | 2 | 另有 `ishikawa.useMaxWidth`/diagramPadding |
| venn | 10 | 5 | 2 | 另有 look + handDrawnSeed |
| treemap | 16 | 6 | 0 | `treemap.valueFormat` 语义 |
| state-diagram-v2 | 51 | 6 | 0 | layout/elk/useMaxWidth |
| 其余 | — | ≤5 | 0–3 | cynefin 布局参数、xychart rotation 3 条历史债（不碰） |

## 4. 成本评估与选择

| 候选 | 可达行 | 机制面 | 风险 |
|---|---|---|---|
| **pie** | **5** | **1**：在既有 `pie_config_frontmatter`/折叠 description profile 旁新增一个 `PieTheme` profile（同一 TU `mermaid_charts_impl.cpp`），页脚在 `charts_layout` 追加 | 中低：charts TU 已被 RFC 0035 安全扩展过一次；pie 路径小、无共享契约变更 |
| flowchart | 7 | 1 个但极大（exact-bit profile 机器）且要先动受保护 gap roster | 高（RFC 0036 前车之鉴） |
| ishikawa + venn | 4 | 2（两个家族各一个 gate） | 低但单位成本更差 |
| 其余 | 0 惰性 | 语义/几何 | 不是准入切片 |

**选择：pie，5 行**。主题 ∈ {dark, redux, redux-dark, redux-color,
redux-dark-color}（恰好 5 个 corpus 值）；无 `look`。5 行 = 两种 body（十二片
redux 调色板 × 4 + Sports in Sweden × dark）。corpus twin：dark 行与
`pie/pr-check/simple-sports.mmd` body 逐字节相同；4 条 redux 行互为 body 等价但
**没有已绘制等价物**，用合成 config-free twin（RFC 0039 同例）。

## 5. 目标与副作用（预期）

- pie 7/17 → **12/12**；全局 1471/674 → **1476/669**；formal 1285 → **1290**；
  gaps 48、integrity 0 不变。
- roster 674 → **669**（`family_residual:pie` 17→12，类别非空；`--self-test` 键集不变式安全）。
- 5 行均不在受保护 `e6c` gap roster（该 roster 仅 class/flowchart）。
- narrow_40 必须 live 实测：按规则 34，页脚行（64 字符）超过 40 列视口，预计 0/5 可绘，
  以门禁实测为准。

## 6. 三笔历史债（本次不动）

1. xychart rotation 3 行。
2. narrow_40 全回退。
3. ER107 B3 三门。

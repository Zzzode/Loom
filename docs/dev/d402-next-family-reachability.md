# D-402 —— 下一家族选择：按「可达 drawn 增量 / 成本」实测（git 家族）

日期 2026-11-22 · 本地，未 push · 承接 Tech Lead 指令（RFC 0038 收口后继续下一家族；
判据为「可达 drawn 增量 / 实现成本」，**不是**按原始 fallback 数排序）与 D-400 的方法。

## 1. 判据

对每个家族只统计**真正可达**的行：把整块 frontmatter 去掉后 body 能在宽 240 原生绘制，
且 frontmatter 里除已建立的闭合表（`look`/`theme`）与已准入叶子之外没有别的阻塞键；
再剔除冻结 gap fixture 与 body 能力标记（图标字体 `fa:`、`@{shape}` 等 faithfulness 阻塞）。
最后用「每单位成本」（新增机制面：需要新逻辑的文件/契约数）去除可达行数。

## 2. 探针方法（可复现）

一次性 gtest（运行后删除，方法记录于此）：对 `tools/mermaid/fallback_roster.json` 的
全部 **726** 条 fallback 行渲染两次——原样、以及去掉整块 frontmatter——宽 240、高 400。
判定谓词是官方矩阵使用的 **cell surface**（字符 + 背景色；前景色忽略）。日志
`/tmp/reach_probe_d402.log`（261 行 `strip=1`）。

> 方法教训：探针第一版用 `Screen::ToString()`（含前景色）比较，把 318 条 fallback 行
> 误判为「原样可绘」——fallback 代码块与 text 代码块的前景色本来就不同。改用 cell
> surface 后 `full=1` 归零。凡「是否绘制」的探针必须用矩阵谓词，不能用 ToString。

## 3. 实测结果（fallback 行数 / 去 frontmatter 后可绘）

| 家族 | fallback | strip 可绘 | 仅 look/theme 的干净子集 | 备注 |
|---|---|---|---|---|
| flowchart | 240 | 67 | **19** | 内含 4 条 `fa:` 图标字体与 4 条 `@{shape}`，faithfulness 阻塞 → 忠实上限约 11（其中 8 条为 redux* subgraph theme-only） |
| git | 77 | 50 | **12** | 形状类同 18 条，其中 6 条 body 能力阻塞（theme color 循环上限） |
| requirement | 28 | 26 | 0 | 剩菜全为 elk/命名空间/畸形 |
| timeline | 23 | 16 | **16** | 全部 `look`+`theme`（含 dark/forest/neutral） |
| pie | 17 | 13 | 5 | 其余 8 条 `pie.*` 语义键 |
| agentflow | 11 | 11 | 9（+2 `handDrawnSeed`） | theme ∈ {base,dark,default,forest,neutral,redux-*} |
| gantt | 13 | 11 | **11** | theme-only |
| er-diagram | 24 | 9 | 5 | ER107 冻结权威 + B3 三门 |
| journey | 8 | 4 | 4 | 与 timeline/gantt 共用颜色门 |
| ishikawa / venn / conf / railroad | — | — | 3/3/1/1 | 无信封，量太小 |
| class / swimlanes / sankey / usecase / quadrant / katex / wardley / info / zenuml | — | 0 | 0 | 无 body 能力或全冻结 |

## 4. 成本评估与选择

| 候选 | 可达行 | 机制面（新增/改动的逻辑面） | 风险 |
|---|---|---|---|
| **git** | **12** | **1**：扩展现有的 40 行 `git_frontmatter_body` 信封（已准入 description）；披露行在 `gitgraph_layout` 内部追加到自己的画布 | 低：单家族、无共享模块、无跨家族不变式；闭合表在 RFC 0037/0038 已两次评审 |
| timeline（单独） | 16 | 2：新增信封 + 披露通道 | 中：与 gantt/journey 共用「no color contract，三者不得漂移」的闸门，单改一个家族即制造漂移 |
| timeline+gantt+journey | 31 | 4：共享校验器 + 3 个渲染器 + 闸门改写 | 中高：共享模块变更，需要独立的跨家族 RFC |
| flowchart | 8–11 | 1 个但极大：2800 行 exact-bit profile 机器需新增 4 类 profile 形状并放宽 `look: neo` | 高：该机器的安全性质是「每个 profile 单独论证」；RFC 0036 曾因测量被证伪而撤回 |
| agentflow | 9 | 1：自有 `af_frontmatter_gated` | 中低：theme 含 base/dark/forest/neutral |
| er | 5 | 1：ER 已有信封，但触碰 ER107 冻结权威与 B3 三门 | 高 |
| pie | 5 | 1：无信封 | 低但增量最小 |

**选择：git 家族，12 行**（`look: neo`；theme ∈ {neo, neo-dark, redux, redux-dark,
redux-color, redux-dark-color}）。理由：单位成本下增量最高（12 行 / 1 个机制面），
且没有任何共享契约或跨家族不变式需要改动；flowchart 的可达上限（8–11）低于 git 且
风险最高，timeline 三家族的 31 行应作为独立的共享机制 RFC 处理，不塞进单家族改动。

git 的 12 行形状类为 18 行，其余 6 行 body 能力阻塞——该切分是**渲染定义**的，因此
实现时采用「评审过的字面路径集 + 整组摘要」，生成器只断言源码可推导计数（家族 135、
形状类 `look-theme` 18、`nested config.gitGraph.*` 44、cohort 12），与 RFC 0038 同一纪律。

## 5. 三笔历史债（本次不动）

1. xychart rotation 3 行（需要真实几何）。
2. narrow_40 全回退（宽屏准入不是窄屏承诺）。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/`kB3ReviewStatus`、
   `WRITE_REVIEW_APPROVED`）。

## 6. 复现

- 探针源码：`tests/ui/test_mermaid_family_reach_probe.cpp`（一次性，已删除并撤销注册；
  注册方式与 RFC 0037/0038 的 cohort 门一致，`ctest -L mermaid`）。
- 日志：`/tmp/reach_probe_d402.log`（release 预设运行，迭代期用 release，按效率规则）。

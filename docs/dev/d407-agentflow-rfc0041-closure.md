# D-407 —— RFC 0041（agentflow look/theme 准入）实现收口记录

日期 2026-11-22 · 分支 master · 起点 HEAD `79180d92`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0041 已标记 **implemented**（单阶段完成、
全量 debug ctest 一次完整运行、双预设矩阵字节一致、无受保护文件改动）。

## 1. 家族与选择依据（D-406 / TL 判据）

按「可达 drawn 增量 / 每单位成本」（D-406，复用 D-402 strip 探针日志并剔除
RFC 0039/0040 已晋升的 43 行，683/683 覆盖、与当前矩阵逐家族交叉校验）：剩余
218 条 strip 可绘中，agentflow 有 **9 条惰性 theme-only**、机制面 1 个；
flowchart 8–11（exact-bit 机器，最高风险）、pie 4、venn/ishikawa 2；git 38 /
requirement 26 / state-v2 6 属语义/几何能力而非准入。

家族结构（实测）：38 = 27 已绘制 + **9 theme-only 可达** + 2 `look`+`handDrawnSeed`
（seed 决定几何，非惰性，保持回退）。9 行 = 两种 body × 主题；全部 body 与两条已绘制
fixture 逐字节相同（5 → `10-every-container-kind`，4 → `15-support-triage`），因此账本
钉 **corpus twin**。

## 2. RFC 生命周期（agent-run 门）

| 门 | 评审 agent | 结论 |
|---|---|---|
| 设计门 r1 | `agent:a255f982730d3c06d`（独立复算分区/端点，并对两版 gate 建模型实测 38 条 fixture） | REQUEST-CHANGES（1 BLOCKER + 3 MAJOR + 4 MINOR + 3 NIT） |
| 设计门 r2 | 同一 agent | APPROVE-TO-ACCEPT |
| PRR r1 | `agent:a2a24cca22d776afd`（毕业可测性/隐藏测试/构建/Work/false-green/回滚/耦合/先例） | APPROVE-TO-IMPLEMENT（1 MAJOR + 4 MINOR + 3 NIT 折入） |
| P1 代码评审 | `agent:a2ce69cbc57153f56`（只读对抗） | **APPROVE**（0 BLOCKER/MAJOR；3 MINOR + 2 NIT 已改） |

关键修正在落地前完成：
- **设计 r1 BLOCKER**：既有 `test_mermaid_agentflow_cohort.cpp` 未入改动集且
  `gated_count==11` 与 3 条负例会转红 → 变更集与测试计划补齐。
- **设计 r1 MAJOR**：「严格放宽」原表述不实（今天所有缩进行被跳过）→ 新解释仅限
  config 块内；config 含准入叶子 + 其他叶子会 fail-open → 改为「必须恰好一个准入叶子」；
  9 行 body 与已绘制 fixture 等价 → 改用 corpus twin。
- **PRR MAJOR**：新门禁未列 **live narrow_40** 断言 → 加入逐行实测断言，禁止照抄 twin。
- **P1 评审**：实现全部通过；3 个 MINOR（生成器 docstring、RFC 窄屏成因表述、家族测试
  头注释）已改。

## 3. 实现（单个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `1d729fe3` | `af_frontmatter_gated` 改为块上下文校验 + 页脚（`Agentflow native projection v1`）；9 行晋升；家族账本三路分区 27+9+2；corpus-twin 账本（digest `5874b783…`）；6 gtest + 2 hermetic `--check`；矩阵分支；家族测试边界更新；roster 683→674 |

生产改动仅 `mermaid_agentflow_impl.cpp` 一个实现单元（无签名/模块/导入变更，
BMI 不变；`AfDiagram` 增加一个 TU 内 `notices` 成员）。

## 4. 普查与字节门

| 指标 | 起点（RFC 0040 收口） | 收口（P1） | Δ |
|---|---|---|---|
| agentflow wide_240 drawn / fallback | 27 / 11 | **36 / 2** | +9 / −9 |
| 全局 wide_240 drawn / fallback | 1462 / 683 | **1471 / 674** | +9 / −9 |
| formal `drawn_at_240` | 1276 | **1285** | +9 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40 drawn（agentflow） | 19/19（27 行内） | **19/19**（+9 行全部 `source`） | 0（实测，见 §5） |
| fallback roster | 683（44 类） | **674（44 类）** | −9 |

矩阵 sha256：debug 与 release 逐字节一致
`326fb96f00c73516b7bd3ddb1397563ff7d0aed9208e25c839f1f60c1eeb6853`。
Roster 迁移唯一 diff = 9 条被晋升行（`edge_animation_metadata` 91→82；matched 242 /
drawn 151→160 / fallback 82；compact candidates 不变；类别非空，键集不变式安全）。

## 5. narrow_40 实测推翻了设计期预测

设计期按 corpus twin 预测 5 条容器类行在 40 列可绘（PRR 要求 live 断言正是为此）。
实现后门禁实测：**9 行全部在 40 列整源回退**。原因有二：页脚最长行
（`config.theme=<v> -> colour palette; not applied`）超过 40 列视口，使 5 条容器类行
（其无页脚的 twin 在 40 列可绘）在准入后回退；4 条 triage 行原本就过宽（其 twin 的
`narrow_40` 即为 `source`）。账本记录实测值、门禁逐行断言，RFC 的 OQ-3 以实测结案。

## 6. 三条债务（未变）

1. xychart rotation 3 行（需要真实几何）。
2. narrow_40：所有家族仍回退；宽屏准入不是窄屏承诺。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/`kB3ReviewStatus`、
   `WRITE_REVIEW_APPROVED`）零改动。

## 7. 验证

- debug 定向：新门禁 6/6 gtest + 2 hermetic `--check`；家族门禁 5/5；合计 13/13。
- debug 全量 `ctest -L mermaid`：**1465/1465**，`MERMAID_EXIT=0`（上一轮 1457，
  +8 = 6 gtest + 2 hermetic；PRR 预估 +9，以实测 8 结案）。
- release 矩阵：exit 0，27.16 s；debug 与 release 的
  `mermaid-official-matrix.json` sha256 相同（见 §4）。
- 全量 debug ctest 一次完整运行：见 §8。

## 8. 全量门与 flake 处置（D-401 §7 纪律）

**唯一一次完整全量运行**（debug 预设，`-j8`，日志
`/tmp/ctest-full-dbg-0041.log`）：

- `100% tests passed out of 4650`，`FULL_CTEST_EXIT=0`，real 541.83 s。
- 无失败项，因此无需 flake 举证或局部重跑；skipped/disabled 项均为既有的平台级
  跳过（Windows/PowerShell、macOS 剪贴板、PTY 不可用环境等），集合与上一轮一致。

未运行 ASan 全量：本轮不触碰 ER107 冻结面与任何接口，且按效率规则 6，巨头冻结门在
ASan 下不可行属于既有的测试架构债（`docs/dev/engineering-efficiency.md`
「Open efficiency work #6」），不在本次家族收口范围内重演。

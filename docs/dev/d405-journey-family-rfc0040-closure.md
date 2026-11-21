# D-405 —— RFC 0040（journey 三兄弟 look/theme 准入）实现收口记录

日期 2026-11-22 · 分支 master · 起点 HEAD `0df0488f`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0040 已标记 **implemented**（单阶段完成、
全量 debug ctest 一次完整运行、双预设矩阵字节一致、无受保护文件改动）。

## 1. 家族与选择依据（D-404 / TL 判据）

按「可达 drawn 增量 / 每单位成本」实测（D-404，承接 D-402 的 726 条 fallback 全量
探针）：timeline 16、gantt 11、journey 4，三者**共用同一段「不得漂移」的颜色闸门**
（三个 TU 的注释明写 the three renderers must not drift apart here）。因此正确单位是
一个 RFC 覆盖三者（31 行），实现仍按既有惯例各 TU 本地（state/requirement/git 信封
同样是各 TU 本地、以跨家族门禁保证语义一致），不引入共享模块/BMI 变更。

家族内保留：timeline 7 条 + journey 4 条经 **body `%%{init}%%` 主题指令**、
gantt 2 条畸形日期（既有 `fail_closed`）；gantt 3 行 / journey 8 行带
`gantt.*`/`journey.*` 非视觉 config 键，靠既有 pass-through 保持绘制。

## 2. RFC 生命周期（agent-run 门）

| 门 | 评审 agent | 结论 |
|---|---|---|
| 设计门 r1 | `agent:aa0ce043b4adde4a9`（独立复算全部计数与端点） | REQUEST-CHANGES（1 BLOCKER + 6 MAJOR + 5 MINOR + 1 NIT） |
| 设计门 r2 | 同一 agent | REQUEST-CHANGES（4 项残留） |
| 设计门 r3 | 同一 agent | APPROVE-TO-ACCEPT |
| PRR r1 | `agent:ab976b78b004c884a`（可实施性/假绿/构建扇出/DoS/回滚） | APPROVE-TO-IMPLEMENT（12 项折入） |
| PRR r2 | 同一 agent | APPROVE-TO-IMPLEMENT |
| P1 代码评审 r1 | `agent:ada3add4beecf9f9f`（只读对抗评审） | REQUEST-CHANGES（1 BLOCKER + 1 MAJOR + 2 MINOR + 4 NIT） |
| P1 代码评审 r2 | 同一 agent | **APPROVE** |

关键修正在落地前完成：
- **设计 r1 BLOCKER**：`fallback_attribution.py --self-test` 在 `--emit` 后必红
  （`look_classic_other` 恰好是本切片晋升的 6 条 timeline 行，会清空，而 `causality`
  保留全部 detector → 键集不等）→ RFC 要求 `--emit` 对每个 detector 类别都输出
  （可为空），实现按此落地并在 roster 上验证。
- **设计 r2 MAJOR-4/5**：把「any indentation」改为**块上下文行扫描**（仅
  `config:` 内缩进 2 准入），并把标量 reader 钉为 trim→剥注释→去引号，
  `theme: dark<br/>` 因此**准入**并披露 `config.theme=dark`。
- **PRR r1**：矩阵分支必须放在 generic fallback 之前，否则 `observed_gap_rows`
  会从 48 变 79；battery 必须枚举完整兄弟独有值集合；新门禁的「已绘制」谓词必须是
  行相等（drawn body 的 title 真的含 "Gantt"/"Journey"）。
- **P1 评审 r1**：B1 是批准后流程步骤（账本 `pending` 时矩阵分支按设计拒绝加载，
  阶段评审记录即闭合手段）；MAJOR-1 是真实缺口——gantt/journey 未覆盖 RFC §3
  「逐家族至少」探针组 → 重写为一个 `FamilyBattery`/`run_battery()` 对三家族各跑一遍
  （全部准入值、兄弟独有值、unknown/case/empty、重复、引号、注释、未闭合引号、
  indent 0/2/4、`themeVariables`、payload、admitted+body-init，以及**字面量页脚块**
  含空分隔行与 look→theme 顺序）；oracle 钉死 7+39+10=56 行；Python 围栏改为与 C++
  完全一致的精确匹配。

## 3. 实现（单个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `bea4ae5b` | 三 TU 的闭合表 + 校验/披露页脚；31 行晋升；共享 Python 表模块 + 四路分区生成器 + 新账本（digest `cfdb3e80…`）/7 gtest + 4 hermetic `--check` 门禁/矩阵分支/E2a 交互/roster 迁移 |

生产改动仅三个实现单元（`mermaid_timeline_impl.cpp`、`mermaid_gantt_impl.cpp`、
`mermaid_journey_impl.cpp`），无签名变更、无新模块/导入、BMI 不变；三份 helper
（136 行）逐字节一致，仅表与页脚 header 不同。

## 4. 普查与字节门

| 指标 | 起点（RFC 0039 收口） | 收口（P1） | Δ |
|---|---|---|---|
| timeline wide_240 drawn / fallback | 7 / 23 | **23 / 7** | +16 / −16 |
| gantt wide_240 drawn / fallback | 39 / 13 | **50 / 2** | +11 / −11 |
| journey wide_240 drawn / fallback | 10 / 8 | **14 / 4** | +4 / −4 |
| 全局 wide_240 drawn / fallback | 1431 / 714 | **1462 / 683** | +31 / −31 |
| formal `drawn_at_240` | 1245 | **1276** | +31 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40 drawn（三家族） | 0 | **0** | 0（无承诺；逐行由账本实测） |
| fallback roster | 714（44 类） | **683（44 类）** | −31 |

矩阵 sha256：debug 与 release 逐字节一致
`fd852b74a8dbc0817c20c89b0f9a743f51aa1799effe3414b1258d31298d20fd`。
Roster 迁移唯一 diff = 31 条被晋升行（分类 `family_residual:gantt` −11、
`family_residual:journey` −4、`look_classic_other` −6、`look_neo` −10），
`look_classic_other` 清空但类别仍发射（设计 r1 BLOCKER 的约定）；`--self-test`
与 `--matrix` drift gate 均绿。

## 5. 三条债务（未变）

1. xychart rotation 3 行（需要真实几何）。
2. narrow_40：所有家族仍回退；宽屏准入不是窄屏承诺。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/`kB3ReviewStatus`、
   `WRITE_REVIEW_APPROVED`）零改动。

## 6. 验证

- debug 定向：新门禁 11/11（7 gtest + 4 hermetic `--check`）；三家族既有 cohort
  26/26；评审 agent 复跑合计 37/37。
- debug 全量 `ctest -L mermaid`：1457/1457，`MERMAID_EXIT=0`，real 482.83 s。
- release 矩阵：`MermaidCapabilityReport.OfficialMatrix` exit 0，27.81 s；
  debug 与 release 的 `mermaid-official-matrix.json` sha256 相同（见 §4）。
- 全量 debug ctest 一次完整运行：见 §7。

## 7. 全量门与 flake 处置（D-401 §7 纪律）

**唯一一次完整全量运行**（debug 预设，`-j8`，日志
`/tmp/ctest-full-dbg-0040.log`）：

- `100% tests passed out of 4642`，`FULL_CTEST_EXIT=0`，real 546.01 s。
- 无失败项，因此无需 flake 举证或局部重跑；skipped/disabled 项均为既有的平台级
  跳过（Windows/PowerShell、macOS 剪贴板、PTY 不可用环境等），集合与上一轮一致。

未运行 ASan 全量：本轮不触碰 ER107 冻结面与任何接口，且按效率规则 6，巨头冻结门在
ASan 下不可行属于既有的测试架构债（登记在 `docs/dev/engineering-efficiency.md`
「Open efficiency work #6」），不在本次家族收口范围内重演。

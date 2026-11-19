# D-383 — RFC 0034 Phase 4 收口记录（一页式）

日期 2026-11-19 · 分支 master · HEAD `603ebdd8` · 领先 origin/master **95**，**未 push / 未 merge / 生产行为零改 / 十保护文件零改**。

## 1. 两个提交（合并口径覆盖 D-383 四要素）

| 提交 | 类型 | 要素 |
|---|---|---|
| `ea7de75e` | test(mermaid) | E1 cohort 退休、E2 verifier 退役断言、E3 roster 907→906（6 文件，test/tools） |
| `603ebdd8` | docs(rfc) | E4 超替注记 + RFC implemented/PRR + design-decisions + flake 回顾（5 文件，docs） |

## 2. 四要素 → 文件与行号

| 要素 | 位置（行号） |
|---|---|
| E1 cohort 退休（7 draw / 0 blocked / 1 retired，blocker 全 null，retired_blocker 仅审计） | `tools/mermaid/er-group-edge-cohort.json:26-28`（counts）、`:33`（retired vocabulary）、`:246-249`（handDrawn 行 `retired_by:"RFC 0034"`）；生成器漂移守卫 `tools/mermaid/generate_er_group_edge_cohort.py:91,169,200-225` |
| E2 verifier fail-closed 退役断言 | `tools/mermaid/verify_er_groupedge_admission.py:266`（`assert_er198_retirement`）、`:40-41`（contract + proposal sha 4e2ba986…）、`:424`（只读路径）、`:460`（unittest）；自检 9/9 |
| E3 fallback roster（仅工具再生成） | `tools/mermaid/fallback_roster.json:4`（total 906）、`:44`（look_handdrawn 70）；工具 `tools/mermaid/fallback_attribution.py:215`（emit）、`:389`（drift gate）；checked-in 与 `--emit` 逐字节相同，其余 43 分类不变 |
| E4 文档超替（历史快照保留，无测试钉住） | `docs/dev/mermaid-failure-inventory.json:15711`（`rfc0034_supersession`）、`docs/dev/mermaid-unsuccessful-cases.md:179`（SUPERSEDED 行）；另 `docs/rfcs/0034…md:4` status implemented、`:310` PRR 十项表、`docs/decisions/design-decisions.md:145` 两池 meter 条目 |

## 3. 双 preset 全矩阵字节门

| preset | gtest XML（tests/fail） | 矩阵 sha256 |
|---|---|---|
| debug | `matrix-debug.xml` 1/0 | `ee743515f2e2a68b8595328ed2d101f73430a344e75056707a3e3a59d8dd2ab1` |
| release | `matrix-release.xml` 1/0 | `ee743515f2e2a68b8595328ed2d101f73430a344e75056707a3e3a59d8dd2ab1` |

`cmp` 逐字节相同。2738 例仅 ordinal-198 变；original wide/restored **drawn 1239 / fallback 906 / error 0**，`mandatory_gaps=48`，`integrity_errors=[]`；198 宽度 XOR（240 drawn / 40 source），legacy `expected=pending` 保留。

## 4. 十保护文件 sha（前 == 后，10/10 IDENTICAL）

`/tmp/d373-er198-4/protected-before.sha` 与 `protected-after.sha` `cmp` 相同；两提交对保护清单交集为空。短 sha 前后逐对一致：
`3aeb3312 / 1593280f / 93904c1a / 5a1c642c / f8f7dedc / 8f35938e / 76d54ecb / 1fe297da / ce9d3fd7 / 3c0135fd`（ER107 三支持头、三 proposal/audit、ER206 config62、e6c roster、relations oracle、gap roster）。

## 5. 计数口径（以本轮 JUnit 实测为准）

全量 ctest JUnit `ctest-debug-junit.xml`：**注册 4606 = 4570 PASS + 25 运行期 SKIP + 9 FAIL + 2 编译期 DISABLED**。
- ctest 报告分母 **4604 = 4606 − 2 DISABLED**（`FullscreenLayout.StickyPromptClickFiresCallback`、`FullscreenLayout.NewMessagesPillClickCallback`，编译期不执行）；25 SKIP 在 4604 内（BashDanger 16、ClipboardImage 3、CompactionLive 2、Tools 1、E2E_SubagentLive 3）。早期口径若出现 4604，即“执行分母”而非“注册总数”，差异仅这 2 个 DISABLED，无其他来源。
- 作用域内：mermaid 标签 **1419/1419 全绿**（独立 JUnit `ctest-mermaid-label-junit.xml`，0 fail/error/skip）；全量内 1411 个 mermaid 用例零失败。

## 6. 7 例 ×3 轮 `ctest -j1` 隔离复跑（load1 全程 16.3–20.5）

数据 `targeted-reruns.csv` / `targeted-reruns.log`。双峰特征：PASS 均 0.3–0.6s；PTY FAIL 全部精确卡 ~10.2s（harness 固定 10s `terminal.wait()` 截止）；BridgeDaemon 稳定 ~12s。

| 用例（ctest #） | 3 轮结果 | 失败率 | 根因证据 | 跟踪 owner |
|---|---|---|---|---|
| McpStdio.WatchdogKillsSilentChild (#175) | PASS/PASS/PASS | 0/3 | 仅全量并行时失败 1 次，隔离全过；看门狗调度抖动 | 环境，无需改码 |
| BridgeDaemon.ConsecutivePollFailures… (#3796) | FAIL/FAIL/FAIL（~12.03s） | 3/3 | **确定性但非本仓缺陷**：strace `bridge-strace2.log` 实证第 33 个请求是 `User-Agent: persona-ports` 的外来 `GET /`（`~/.persona-devbox-cn` persona-link pid 1174967 / persona-node 1177585 扫描临时端口）；客户端恰 32 connect（8 poll×4 retry）；同二进制 12/13 兄弟通过 | 主机环境（port scanner），断言不改 |
| E2E_Pty_ProgramStatus_question (#4589) | FAIL/PASS/FAIL | 2/3 | 10s 截止超时，非确定性 | PTY 隔离策略（见下） |
| E2E_Pty_Question_layout (#4594) | FAIL/FAIL/FAIL（10.7–11.4s） | 3/3 | 同上；RFC 0017（2026-10-14）已记录该例在“无生产变更”下机器时序 flake | PTY 隔离策略（见下） |
| E2E_Pty_Question_edit (#4595) | FAIL/FAIL/PASS | 2/3 | 同上，非确定性 | PTY 隔离策略（见下） |
| E2E_Pty_Question_default (#4598) | PASS/FAIL/PASS | 1/3 | 同上，非确定性 | PTY 隔离策略（见下） |
| E2E_Pty_Question_empty (#4599) | FAIL/FAIL/FAIL（~10.25s） | 3/3 | 同上；高负载下高概率，但仍是截止超时非逻辑断言 | PTY 隔离策略（见下） |

PTY 聚合 **4/15 通过、11/15 失败（73%）**，全部为截止超时；93 个超前提交与工作树对 tests/e2e、tests/bridge、tests/core/test_mcp、src/daemon、src/bridge、src/utils/http 的改动交集为 **空**。
**跟踪 owner = 本 agent**：PTY harness 10s wall-clock deadline 在 taskset/负 nice 均不可用（127）共享机上的隔离策略（如低负载窗口复跑、deadline 可配置化评估、或 CI 专用节点），列入**下一里程碑**，不阻塞本 Phase。

## 7. rfc_lint 状态

`rfc_lint-final.stdout.log`：**FAILED（exit 1），11 项全部归属 RFC 0033**（重复编号、两个 slice 文档缺 frontmatter、主 RFC 缺六个必需章节、docs/README 缺索引）；与 Phase 4 前快照 `rfc_lint.stdout.log` `diff` **逐字节相同**，RFC 0034 匹配数 **0**，且四个失败文件均不在本两提交内（最后改动 commit 3511a058 / fc642506 / c9ba8c39，2026-11-17/18）。**登记为独立文档修复任务（RFC 0033 companion 文档合规），不属 RFC 0034 范围，本 Phase 不处理。**

## 8. 边界确认

不 push、不 merge；Phase 4 无 `src/` 生产改动（两池 meter 在 Phase 1 `27103a53`）；无持久化/wire/settings/session 契约变更；十保护文件零改；对抗式评审 APPROVE（1 Minor+2 Nit 已在 `ea7de75e` 内闭环）。全部证据目录 `/tmp/d373-er198-4/`。

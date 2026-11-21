# D-403 —— RFC 0039（gitGraph look/theme 准入）实现收口记录

日期 2026-11-22 · 分支 master · 起点 HEAD `176ff020`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0039 已标记 **implemented**（单阶段完成、
全量 ctest 一次完整运行 CTEST_EXIT=0、双预设矩阵字节一致、无受保护文件改动）。

## 1. 家族与选择依据（D-402 / TL 638 判据）

按「可达 drawn 增量 / 每单位成本」实测（D-402，726 条 fallback 全量探针，日志
`/tmp/reach_probe_d402.log`）：flowchart 19（其中 8 条 icon-font/`@{shape}` 非忠实 →
上限约 11）、timeline 16、**git 12**、gantt 11、agentflow 9、er 5、pie 5。

选 git：12 行对应 **1 个机制面**（扩展现有 `git_frontmatter` 信封），无共享模块、
无跨家族不变式；flowchart 的 exact-bit profile 机器风险最高（RFC 0036 曾因测量被证伪
撤回）；timeline+gantt+journey 共享「不得漂移」颜色门，应作为独立的共享机制 RFC。

家族内保留：44 条命名空间 `config.gitGraph.*`（语义债）、6 条 body branch/route
density 阻塞、1 条无 look/theme、其余 14 条无 body 能力。

## 2. RFC 生命周期（agent-run 门）

| 门 | 评审 agent | 结论 |
|---|---|---|
| 设计门 r1 | `agent:acde9271b5b1810a9`（独立复算全部测量与端点） | REQUEST-CHANGES（2 MAJOR + 7 MINOR + 3 NIT） |
| 设计门 r2 | 同一 agent | APPROVE-TO-ACCEPT |
| PRR r1 | `agent:ab2865cf0bfe0c5df`（可实施性/假绿/构建扇出/DoS） | REQUEST-CHANGES（1 MAJOR + 9 MINOR + 2 NIT） |
| PRR r2 | 同一 agent | APPROVE-TO-IMPLEMENT |
| P1 代码评审 | `agent:ab81d8d24feb6dc37` | APPROVE（2 MAJOR = 批准后流程步骤；2 LOW + 2 NIT 已改） |

关键修正在落地前完成：
- **设计 r1 MAJOR-1**：页脚 header 不能写小写 `gitGraph`——三个消费者以
  `find("gitGraph") == npos` 作为「已绘制」助记符（e2a / gitgraph_semantics /
  e2e_pty_shutdown），会把 12 条晋升行判成回退 → 改 `GitGraph native projection v1`
  并把新门禁谓词钉为行相等。
- **设计 r1 MAJOR-2**：页脚组成、96 字节上限、发射计费
  `(1+n)*(4*sizeof(string)+256)+bytes*2+256` 与 `art+1+n ≤ 1024` 守卫必须与
  state/flow 路径逐字一致 → 写入 RFC，实现按此落地。
- **PRR r1 MAJOR-1**：第 5 个测试原为「E2a byte-identity」但没有能看见「多余尾部空行」
  的 oracle → 改为**帧恒等 oracle**（58 条 E2a 已绘制 git 行，原源 vs 去 frontmatter
  源，同一 sentinel wrapper，完整未裁剪 cell surface 逐格相等 + 页脚标记缺席）。
- **阶段评审 LOW-3/LOW-4**：生成器补齐与渲染器一致的标量归一化；新增 hermetic ctest
  `MermaidGit0039Cohort.GeneratorIsReproducible` 自动执行生成器 `--check`（cohort 钉值
  首次纳入自动门禁）。

## 3. 实现（单个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `66d0c942` | 闭合 `look`（`neo`）与 `theme`（6 调色板）表；页脚追加在 `gitgraph_layout` 内；12 行晋升；生成器/账本（digest `46d27e97…`）/5+1 门禁/矩阵分支/E2a 负例/roster |

生产改动仅 `mermaid_gitgraph_impl.cpp` 一个实现单元（无签名变更、无新模块/导入）：
1 条编译边 + 1 条归档重链 + 207 条命名该归档的重链边（PRR 实测），BMI PSS N/A。

## 4. 普查与字节门

| 指标 | 起点（RFC 0038 收口） | 收口（P1） | Δ |
|---|---|---|---|
| git wide_240 drawn | 58 | **70** | +12 |
| git wide_240 fallback | 77 | **65** | −12 |
| 全局 wide_240 drawn | 1419 | **1431** | +12 |
| 全局 wide_240 fallback | 726 | **714** | −12 |
| formal `drawn_at_240` | 1233 | **1245** | +12 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40 drawn（git） | 0 | **0** | 0（无承诺；逐行由门禁实测） |
| fallback roster | 726（44 类） | **714（44 类）** | −12 |

矩阵 sha256：debug 与 release 逐字节一致
`9d925815e94c5fbb7447b7cd33b2ad8deebdf81498c4c1600523ee0ba548c80c`。
Roster 迁移唯一 diff = 12 条被晋升 git 行；`--self-test` 与 `--matrix` drift gate 均绿。

## 5. 三条债务（未变）

1. xychart rotation 3 行（需要真实几何）。
2. narrow_40：所有家族仍回退；宽屏准入不是窄屏承诺。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/`kB3ReviewStatus`、
   `WRITE_REVIEW_APPROVED`）零改动。

## 6. 验证

- 定向：`test_mermaid_gitgraph_0039_cohort` 5/5（含 12 行 twin 惰性、两张闭合表、
  页脚固定序、**58 行帧恒等 oracle**）、hermetic 生成器 `--check` 1/1、
  `MermaidE2aCohort`（git 根跳过集 == git 账本，剩余 120 行）、`MermaidGitGraphEnvelope`
  与 `MermaidGitGraphSemantics` 60/60。
- `ctest -L mermaid`（除 OfficialMatrix）：**1444/1444**，CTEST_EXIT=0，3530 s。
- 生成器 `--check` 可复现；roster `--emit`（726→714）+ `--self-test` + `--matrix` 漂移门全绿。
- 双预设矩阵：release 27 s / debug 239 s，均通过且字节一致 `9d925815…`。
- **全量 debug ctest（一次完整运行，`-j$(nproc)`，`/tmp/ctest_d403_full.log`）**：
  `ctest -N` 声明 **4633** 项（其中 2 个编译期 DISABLED），运行 **4631** 项，
  **100% tests passed，CTEST_EXIT=0，268 s**（含 25 个运行期 SKIP）。本轮未出现
  DirectConnect/mock-server 时序 flake。

## 7. 验证纪律（沿用 D-401 §7）

全量门只认一次完整运行的结果；失败项必须在「修复」与「flaky 证据」之间二选一，
flaky 证据最低要求为同一用例在干净环境连续重跑 ≥3 次全通过 + 与改动面无关的证据。
本收口的一次完整运行即为绿门（CTEST_EXIT=0），无需 flaky 举证。

## 8. 未消解 / 推迟项

1. 65 条 git fallback 保持：44 条命名空间 `config.gitGraph.*`（语义债：分支命名、
   label 可见性、commit 排序）、6 条 body branch/route density 阻塞、1 条无 look/theme、
   14 条无 body 能力——全部显式 non-goal。
2. 12 行全部使用合成 config-free twin：无 body 等价的已绘制 git 行（实测 0），
   语料对照不存在，faithfulness 依赖 D-204 渲染契约 + twin 门 + 58 行帧恒等 oracle。
3. `look` 表只有 `neo` 一个值：全部 30 条 git look/theme 行均为 `neo`，语料封闭，
   零损耗；未来出现 classic/handDrawn 语料才扩展。
4. **后续技术债（本轮未做）**：把「生成器 `--check` 纳入 ctest」回填给
   `generate_requirement_0037_cohort.py` 与 `generate_state_0038_cohort.py`（两条历史
   账本的钉值目前仍只靠人工 `--check`）。
5. 受保护文件、`manifest.json`、`kManifestSha256`、`e2a-cohort.json`：**零改**；
   无 wire/持久化/settings/session 契约变更。

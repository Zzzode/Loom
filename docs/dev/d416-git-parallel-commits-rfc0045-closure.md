# D-416 —— RFC 0045 gitGraph `parallelCommits` 准入收口记录

日期 2026-11-22 · 本地，未 push · 承接 TL 常驻授权（RFC 0044 PASS 后直接开下一片，
家族由 roster 实测选定，见 D-415）。受保护 43 条与 flowchart exact-bit 222 条未动。

## 1. 结论（三数并列，TL 口径）

| 指标 | 值 |
|---|---|
| ① 绝对 `source_fallback` 总数 | **628**（640 → 628） |
| ② 超出认证 profile / 冻结权威的部分 | **289**（未变：222 exact-bit + 67 受保护/冻结） |
| ③ 本轮净减少 | **12** |

git **70/65 → 82/53**；全局 **1505/640 → 1517/628**；formal `drawn_at_240`
**1319 → 1331**；`mandatory_gaps` 48、`integrity_errors` 0 不变；roster 640 → 628，
`family_residual:git` 46 → 34，类别数保持 43。

## 2. 机制

- 一个 TU（`mermaid_gitgraph_impl.cpp` 的 `git_frontmatter`），在既有信封门上新增
  **一个命名空间子树**：`gitGraph`（indent 2，空值 → `namespace_entry`）下恰好一个
  布尔叶 `parallelCommits`（indent 4，`true`/`false`，经 `git_inert_scalar` 归一化）。
- 页脚新增一行（最后追加）：
  `config.gitGraph.parallelCommits=<v> -> parallel lane compaction; not applied`
  （最长 78 字节 < 96 上限）。上游的并行车道合并从不执行，页脚如实披露。
- 空命名空间由门自己拒绝（`(GitGraphNs && !Parallel)`）：命名空间位会让
  “空 config 块”守卫失效，这一点与解析器语义分离，是门的不变量。
- 0039 的 `excluded[]` 逐行原因是冻结普查，本片不改写；0039 账本新增
  `selection.census_note` 说明“该列表是 2026-11-21 冻结普查，RFC 0045 按计数承认
  其中 12 条”，`counts.namespaced == 44` 保持源形状钉。

## 3. 证据

- **红先**：12 行在 manifest 中全部是 `raw.expected = "source"`,
  `reason = "frontmatter"`；roster 归入 `family_residual:git`（46）。
- **12 条选择完整性**：对 `cases/git` 结构化扫描，唯一子键恰为 `parallelCommits`
  的命名空间行恰好 12 条，含其他内容键（`showBranches`/`showCommitLabel`/
  `rotateCommitLabel`/`mainBranchName`/`mainBranchOrder`）的 32 条全部排除。
- **twin**：无已绘制 git fixture 与队列行 body 等价，12/12 使用 synthetic
  config-free twin（删除 `config:` 行及其后所有 indent ≥ 2 行，直到下一个列 0 键）；
  twin 必须自身绘制（“双方都回退”不算惰性）。
- **门**：`tests/ui/test_mermaid_git_0045_cohort.cpp` 5 个 TEST —— 账本 schema/评审/
  钉（含 digest 重算、manifest sha、path_pin 集合相等、0039 `namespaced == 44`
  与冻结普查一致）、逐行 twin 惰性 + 页脚组合、命名空间正负电池、既有 look/theme
  电池不变、字面分区（135 = 70 + 12 + 53；53 = 34 + 12 + 6 + 1）。
  native-frame 判据只用逐行相等，绝不用 `find("gitGraph")`（页脚本身含该字面量）。
- **既有断言审计**：没有测试直接命名 `parallelCommits`；
  `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource` 隐式钉住这些行为 whole-source，
  本片按账本驱动扩展其排除集（不改断言强度，`git_skipped == git_admitted` 仍相等，
  扫描 120 → 108 行，仍 >100）。`MainBranchName` 负例在语义门与 0039 门中保持拒绝。
- **矩阵**：新分支置于 0039 git 分支之后、泛化回退之前，reason
  `RFC-0045 gitGraph parallelCommits reviewed cohort`，`raw_active_schema` 留空，
  由通用 `originals_drawn` 记 +12；分支要求账本 `review.verdict == "approved"`。

## 4. 验证（收口实测）

- 定向：`MermaidGit0045Cohort.*` 6/6（5 gtest + 1 hermetic `--check`）；
  `MermaidGitGraph0039Cohort.*` 5/5；`MermaidGitGraphSemantics`/`Order.*` 全绿；
  `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource` 绿；两个生成器 `--check` 绿。
- **双预设矩阵逐字节相同**：debug release 两份报告 sha256 均为
  `ca24369e5c3a2117c3afb93463ad4887967e2b280159623a651d9ac6f2408c82`。
  实测：`original ALL wide_240` **1517 drawn / 628 fallback / 0 error**；
  `original git wide_240` **82 / 53 / 0**；`original_drawing_coverage.drawn_at_240`
  **1331**（2145 分母，62.05%）；`mandatory_gaps` 48；`integrity_errors` 空。
- **全量 debug ctest 一次完整运行**（TL 2026-11-22 流程要求：完整 stdout + 退出码
  落盘）：`/tmp/loom-logs/ctest-full-dbg-0045.log`（1136016 B，
  sha256 `abeffb144af627b43d8272da39b0412bf1dfef7376111fba25b374657b057196`），
  `100% tests passed out of 4681`、`CTEST_EXIT=0`。此前的第一次全量运行（同一二进制）
  为 `99% / 2 failed out of 4681`，红项是已知环境 flake
  `ServerMain.DirectConnectSessionsAndWebSocketUsePersistentRoutes` +
  `E2E_Pty_Handoff_HupStress`，在隔离下 ≥3 次干净重跑；第二次全量为另一对
  `ServerMain.DirectConnect*`，同样隔离复跑全通过（负载 load average ≈ 15–16）。
  最终落盘的一次为 100% 绿，故不再以 transcript 反查取证。
- **roster**：`fallback_attribution.py --emit` 得 **628 / 43 类**，
  `family_residual:git` 46 → **34**；`--self-test` 628/43/零未分类/可复现；
  `--matrix` 628 行全匹配、无未登记或改判。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退（本片 12 行亦为 0/12，页脚超出视口）、
ER107 B3 三门未动。

## 6. 评审（agent-run）

- 代码评审 r1（独立只读对抗式 agent）：**REQUEST-CHANGES**。
  - BLOCKER B1：门测试的 twin 断言用整源子串 `find("parallelCommits")`，而
    folded description 正文本身含该词（12/12 误判）。已改为结构化
    `twin_is_config_free`（只查 frontmatter 的 `config:` / `gitGraph:` /
    `parallelCommits:` 行）。
  - B2：账本 verdict 仍为 `pending`（评审记录回写前的预期态）。
  - M1：`upstream.tag/commit/manifest_sha256` 无门钉 → 已补（manifest sha 重算）。
  - M2：生成器镜像缺 tab 缩进与 body init 拒绝（过宽方向）→ 已补；CRLF 与
    look/theme+namespace 组合为“镜像更严”，在 docstring 声明。
  - M3：电池补重复 namespace 与 `mainBranchOrder`；注明重复 leaf 走解析器
    `seen_paths`。
  - M4：`LiteralPartition` 注明本门不强制 roster 同步（收口 `--emit` 负责）。
  - N1：C++/Python twin 规则统一为 indent ≥ 2 + 列 0 `config:`。
  - N2：RFC Review History 增加更正行（0039 账本 diff 还含
    `selection.census_note`）。
- r2：复评 **APPROVE**（对完整门二进制复跑：唯一红项是 verdict 占位，记录在本轮
  批准后写入；全部修复逐条按代码核实）。4 个非阻塞 NIT：评审记录落库（已完成）、
  README/d117 同步（收口完成）、init 检测边角、twin 未断言 `look`/`theme` 残留
  （本队列不含，生成器 `twin_events` 兜底）。

## 7. 效率复盘

- 生成器镜像（efficiency rule 37）：`admits()` 与 live gate 逐条对照后修了两处
  保真度缺口 —— 顶层 `config` 必须为空结构值（解析器对非空 `config: foo` 直接
  拒绝），以及 folded description 的 payload 行规则（只跳过真正的空行；payload 行
  ≥3 字节、恰好两个前导空格、无 tab；`#` 是 payload 文本而非注释）。修后 12 行
  队列与 digest 不变，`--check` 仍绿——说明缺口不影响本片，但镜像现在与门同形。
- 账本 JSON 对象的 `iter()` 不遍历对象（只对数组有效）：`remaining_by_category`
  用逐键 `get_int` 求和，避免“静默 0”造成的假绿。

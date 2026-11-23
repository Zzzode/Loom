# D-436 —— gitGraph `mainBranchName` / `mainBranchOrder`（14 行，兑现语义）

日期 2026-11-23 · 本地，未 push · 承接 TL「按普查表取下一族，一片一提交」。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：D-435 之后 git 余 15 条：8 条体部 `%%{init}%%`、4 条 >8 分支容量、
  1 条 `switch` 体部关键字、2 条 `mainBranchName`/`mainBranchOrder`。前两类是
  外部/容量墙，第三类是体部文法缺口；这两条是唯一还能**兑现语义**的缝隙。
- **实际面积比最初账本大**：第一次矩阵暴露 12 条 `git/visual-check/` 行（`look: neo`
  + `theme` 表 + 主干叶）也被同一改动放行——它们此前只因主干叶而回退。把机读
  账本从 2 行扩到 **14 行**（新增 12 条 look/theme 组合），而不是在渲染器里人为
  禁止该组合。
- **预期降幅**：14 行（git fallback 34 → 20，全局 532 → 518，formal
  `drawn_at_240` 1427 → 1441；roster 桶 `family_residual:git` 15 → 13、
  `git_cherry_pick` 12 → 6、`look_neo` 28 → 22）。
- **判据（语义，D-412）**：不是惰性披露——画面必须真的体现：
  - `mainBranchName: 'trunk'`：主干标签画成 `@trunk`（不再出现 `@main`），且
    `checkout trunk` 解析到主干（主干头是最后一次对主干的提交 `6`）；
  - `mainBranchOrder: 2`：主干列排到两条兄弟分支右侧（`●  4` 的列号大于
    `●  3`、`●  2`），主干标签仍是 `*@main`。

## 2. 落地内容

- `GitBooleanConfig` 增 `main_branch_name` / `main_branch_order`；
  `git_frontmatter` 接受 `gitGraph.mainBranchName`（标签：可带引号对、非空、≤64
  字节、无空格/引号/`∅`）与 `mainBranchOrder`（纯数字 ≤3 位）。
- **主干身份必须在读体部之前生效**：`parse_gitgraph` 增加两个带默认值的参数
  （`main_branch = "main"`, `main_branch_order = -1`），主干分支按配置名与 order
  创建；否则 `checkout trunk` 会当成未知分支而整源回退。这是模块公开签名上的
  向后兼容新增（两个调用点同步更新，旧的两参调用照常编译）。
- 脚注：两叶都追加 honoured 措辞（`-> trunk branch label; honoured`、
  `-> trunk lane order; honoured`），位于既有四条 boolean 之后。

## 3. 门与账本

- 账本 `tools/mermaid/git-mainbranch-d436-cohort.json`（**14 行**）+ 生成器
  `generate_git_mainbranch_d436_cohort.py --check`，digest `11573196…`、
  manifest sha `c966a725…`；行内含 `config` 值、完整脚注与状态。
- 新门 `tests/ui/test_mermaid_git_mainbranch_d436_cohort.cpp` 4 项：账本钉；
  14 行逐字脚注 + 主干标签（改名后不得出现 `@main`）；**canonical 顺序行的列号
  判据**（`●  4` 列 > `●  3` > `●  2`，并断言 `*@main → 4`）；负例（名称带空格/
  空/重复、order 负数/小数/字母/4 位、表外叶、顶层 `mainBranchName`、`checkout
  main` 在主干改名后仍未知、`look: handDrawn` / `theme: default` 组合、init 不绕过）。
- 矩阵：`git_mainbranch_d436_cohort_map()` 分支置于 RFC 0039 git 分支之前，
  规则 38 reach proof。

## 4. 既有断言迁移（本片足迹）

`mainBranchName` 原先在五处被当作「表外叶」的负例，本片改为仍不准入的表外叶
（未删除、未放宽）：

- RFC 0045 门两处列表 → `gitGraph: showTags: true`；
- RFC 0039 门 → `gitGraph: showTags: true`；
- D-424 门 → `colour: red`；
- 语义测试 `ConfigFrontmatterStaysSource` → `showTags: false`。

## 5. 验证（迭代口径）

- 定向：`ctest --preset debug -R 'Git|E2a|MermaidGit'` = **111/111**，
  `CTEST_EXIT=0`（`/tmp/loom-logs/ctest-git-d436.log{,.exit}`）；日志中逐字出现
  `MermaidGitMainBranchD436Cohort.TrunkIdentityAndLaneOrderAreHonoured`、
  `LaneOrderMovesTheTrunkColumn`、`ClosedMainBranchNegativesStayWholeSource`、
  `GeneratorIsReproducible`，均为 `Passed`。release 预设新门 4/4。
- 矩阵（debug）：两次运行——第一次暴露 12 条 look/theme 组合（§1），账本扩到
  14 行后第二次 **64/64 通过**，`drawn_at_240` **1441**、error 0、
  `mandatory_gaps` 48、`integrity_errors` 空（`/tmp/loom-logs/matrix-d436b.log`）。
- roster `--emit`：**518 / 36 类**（`family_residual:git` 15 → 13、
  `git_cherry_pick` 12 → 6、`look_neo` 28 → 22），`--self-test` / `--matrix` 绿；
  E6c 未动。
- 三数并列：①绝对 source_fallback **518** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **14**。

## 6. 未验证 / 剩余风险

- `mainBranchOrder` 只验证了语料里的 2；更大的 order 会把主干推到更右的列，
  可能触及画布宽度上限（保守回退），无语料证据。
- 主干改名与体部 init 的 `mainBranchName` 不会同时出现（init 行被 frontmatter
  门拒绝）；若未来语料同时出现，需要额外的冲突裁决。
- 剩余 git 13 条：8 条体部 init、4 条 >8 分支容量、1 条 `switch`。`look_neo`
  桶里其余 22 条不属于 git，仍属外部引擎面。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

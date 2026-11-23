# D-435 —— gitGraph frontmatter `title`（1 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「按普查表取下一族，一片一提交」。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：D-434 之后，git 的 16 条残余里 8 条是体部 `%%{init}%%` 指令、
  4 条是 >8 分支的体部/调色板、1 条是 `switch` 关键字体部、2 条是
  `mainBranchName`/`mainBranchOrder`（要兑现语义的较大特性）；剩下 1 条只差
  frontmatter 的 `title:`，是当前最干净的本地缝隙。
- **预期降幅**：1 行（git 35 → 34，全局 533 → 532，formal `drawn_at_240`
  1426 → 1427）。
- **判据（语义，D-412）**：`title` 不是惰性披露——它**必须画出来**，作为画面第一行
  `title <text>`；判据是「去掉标题行后的画面 == 去掉 frontmatter title 的孪生画面」。

## 2. 落地内容（单文件 `mermaid_gitgraph_impl.cpp`）

- `git_frontmatter` 增加顶层 `title` 事件：必须在 `description` 之后、`config`
  之前、最多一次；文本为 prose（≤256 字节、无制表符、无首尾空格、Unicode、
  不成对/内部引号一律拒绝，成对引号剥除）。
- `GitEnvelope` 新增 `std::optional<std::string> title`；布局阶段把
  `title <text>` 插到画面**第一行**（受高度与 Work 预算约束），披露脚注仍在画面
  之后不变。
- 体部解析、`config` 封闭表、ASCII 路径均不变。

## 3. 门与账本

- 账本 `tools/mermaid/git-title-d435-cohort.json`（**1 行**）+ 生成器
  `generate_git_title_d435_cohort.py --check`（镜像标题文法），digest
  `e420ab58…`、manifest sha `c966a725…`，宽/窄两档都是 `drawn`。
- 新门 `tests/ui/test_mermaid_git_title_d435_cohort.cpp` 3 项：账本钉；
  该行「绘制 + 首行 == `title simple gitGraph` + 去掉标题行 == 孪生画面」；
  标题正例（带引号标题、标题 + 已准入 config 块且脚注仍在）与负例（空标题、
  重复标题、`config` 之后的标题、制表符、不成对引号、空引号、`theme`/`look`、
  只有 title 没有 description、标题不绕过体部解析）。
- 矩阵：**不新增分支**。该行无顶层 config，本来就归 RFC 0031 E2a 预注册账本
  （"frontmatter 且无 config/init" 的机械选择规则）；E2a 的
  `gitgraph-title-not-projected` 探测器随本片退休（同 D235 先例），账本
  `predicted_draw` 60 → 61、`predicted_blocked` 9 → 8，矩阵仍由 E2a 分支记账。
  新门里加了一条**跨账本一致性断言**：D-435 账本的每一行在 E2a 账本里必须是
  `predicted-draw`，防止两份账本漂移。

## 4. 既有断言迁移（本片足迹）

四处旧契约断言都测的是「title 一律回退」，本片把契约改成「title 画出」，
正例由新门拥有，负例换成仍不准入的形状（未删除、未放宽任何检查面）：

- `MermaidGitGraphEnvelope.TitleIsBlockedNotDropped` →
  `TitleIsHonouredNotDropped`：断言绘制且含 `title simpleGitGraphTitle`，并新增
  空标题 / config 之后 / 重复标题三个负例。
- RFC 0039 git 门：titled probe 的负例改为 `title` + 表外叶 `layout: elk`。
- RFC 0045 git 门：`title stays unadmitted` 负例改为「config 之后的 title」。
- RFC 0031 E2a 门/账本：`gitgraph-title-not-projected` 探测器退休（词表 6 → 5），
  计数改为 draw 61 / blocked 8；`drawn_native` 的「源回显」启发式从
  「全文含 `gitGraph`」收紧为「首行等于表头」——前者会被 `title simple gitGraph`
  误伤，逐像元相等已证明非回显。
- RFC 0039 git 门的 `DrawnGitRowsKeepFrameIdentity`：该判据是「带 frontmatter 与
  剥离后画面**逐字节相同**」，对 title 行不再成立（title 本来就要多画一行），
  因此按 E2a 账本的 `metadata_keys` 排除 title 行（计数仍为 58：退休解锁 +1、
  排除 -1），title 行的精确判据由 D-435 门拥有。

## 5. 验证（迭代口径）

- 定向：`ctest --preset debug -R 'Git|E2a'` = **106/106**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-git-d435.log{,.exit}`）；日志中逐字出现
  `MermaidGitGraphEnvelope.TitleIsHonouredNotDropped`、
  `MermaidGitGraph0039Cohort.DrawnGitRowsKeepFrameIdentity`、
  `MermaidGitTitleD435Cohort.TitleLeadsTheFrameAndArtMatchesTitleFreeTwin`、
  `MermaidGitTitleD435Cohort.ClosedTitleNegativesStayWholeSource`、
  `MermaidGitTitleD435Cohort.GeneratorIsReproducible`、
  `MermaidE2aCohort.LedgerMatchesSchemaAndReviewed`，均为 `Passed`。
  release 预设 D-435 门 3/3、E2a 门 5/5。
- 矩阵（debug，直接跑 `test_mermaid_official`）：两次运行，第一次暴露 E2a
  归属问题（见 §4），迁移后第二次 **64/64 通过**，`drawn_at_240` **1427**、
  error 0、`mandatory_gaps` 48、`integrity_errors` 空
  （`/tmp/loom-logs/matrix-d435b.log`）。
- roster `--emit`：**532 / 36 类**（`family_residual:git` 16 → 15），
  `--self-test` / `--matrix` 绿；E6c 未动。
- 三数并列：①绝对 source_fallback **532** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **1**。

## 6. 未验证 / 剩余风险

- 标题是单行插入，不做折行；语料标题很短。若出现超宽标题，画面行会超过宽度上限
  被帧高度/宽度守卫拒绝（保守回退），需要新语料时重审。
- 剩余 git 残余 15 条：8 条体部 init、4 条 >8 分支、1 条 `switch`、2 条
  `mainBranchName`/`mainBranchOrder`（后者要兑现语义，建议单独一片或走 RFC）。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

# D-424 —— gitGraph 封闭布尔命名空间（18 行）

日期 2026-11-22 · 本地，未 push · 承接 TL「下一片自选家族、不设评审门」授权。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、**ER107 冻结面**、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与预期（TL 要求写进测试文档或提交信息）

- **为什么选它**：上一片收口后 583 条 fallback 中，`family_residual:git` 占 34 条；
  其中 18 条 originals 的 `config.gitGraph` 块**恰好只含四个封闭布尔叶**
  `{showBranches, showCommitLabel, rotateCommitLabel, parallelCommits}`，值均为精确
  `true`/`false`。这是源自身的语义状态标记（隐藏分支引用 / 提交 id），不是渲染差异
  代理量；git 家族已有三个已验收 cohort 与现成的矩阵分支模式，改动可收敛为一个
  信封分支加两处绘制开关。
- **预期降幅**：18 行（git 53 → 35，全局 583 → 565，formal `drawn_at_240`
  1376 → 1394）。
- **判据（语义，D-412）**：`showBranches: false` 的帧里不出现任何分支引用文本，
  `showCommitLabel: false` 的帧里没有任何提交标签；同一 body 的可见 twin 必须出现
  全部显式 id 与分支引用；`rotateCommitLabel`/`parallelCommits` 在终端无对应物，
  只做披露，其 true/false 画布必须逐字节相同。
- **编号口径**：本片按 TL 豁免不开 RFC，因此不以 `RFC 0049` 命名（避免指向不存在的
  RFC），统一用决策号 **D-424**：账本 schema、生成器、测试套件、矩阵 reason 字符串。

## 2. 落地内容

- **封闭命名空间**：`config.gitGraph` 仅接受上述四个叶，精确 `true`/`false`
  （引号与行尾注释经 `git_inert_scalar` 归一）。未知叶（`mainBranchName`、
  `mainBranchOrder`）、非布尔值、重复键、错误缩进、命名空间外同名叶全部整源回退。
  RFC 0045 的 `parallelCommits`-only 行维持原分支与页脚字节序。
- **真正隐藏，而非留空**：隐藏的分支引用/提交 id 不贡献任何单元格——caption
  置空、`plan_parent_routes` 不预留 caption span（零长度预留会被
  `reserve_parent_span` 拒绝，导致整个布局回退）、`draw_branch_refs` 只画 lane 引导。
  因此几何按源的意图收缩，不会留下空白标签槽。
- **两个惰性布尔只披露**：`rotateCommitLabel`（终端无旋转文字）与
  `parallelCommits`（RFC 0045 已证惰性）输出 `-> … not applied` 页脚，且其
  true/false 对画布必须相同。
- **页脚顺序是兼容面**：三条新行插在扁平 look/theme 叶之后、RFC 0045 的
  `parallelCommits` 行之前，已绘制的 git 帧页脚保持逐字节不变。

## 3. 门与账本

- 新账本 `tools/mermaid/git-bool-d424-cohort.json`（**18 行**），渲染器无关生成器
  `generate_git_bool_d424_cohort.py`（`--check` 隐式门）。钉：cohort digest
  `87d27aeb…`、manifest sha `c966a725…`；counts `{total 18, hidden_branches 4,
  hidden_labels 4}`；逐行记录 `booleans`、`semantics{commit_ids, branch_names}`、
  页脚行（header + 四行，规范顺序）。
- 新门 `tests/ui/test_mermaid_git_bool_d424_cohort.cpp` 4 项：
  1. 账本 schema/decision/counts/摘要/manifest 钉 + 逐行 sha 与页脚逐行重建；
  2. 18 行宽 240 绘制、native 帧、40 列回退、恢复绘制、页脚精确；
     11 行「画布 = 去 config twin 画布」逐行相等，7 行（含 1 行双重隐藏）不等；
  3. 语义 oracle：提交标签的判定是**结构性的**——从标签字形向左跨越
     `─│┘┐└┌┬┴├┤` 与空格能到达提交标记 `●` 才算 caption；`*@main → 5` 在 `→`
     处停下，因此「扫数字」不会误判。隐藏行断言无 caption（含 `merge` 的隐式
     `0`），可见 twin 断言每个显式 id 都是 caption；隐藏分支行断言分支名整帧不出现；
     rotate/parallel 两对画布相等；
  4. 命名空间负例 10 条（未知叶、`yes`/`TRUE`/数字/空值、重复、缩进错误、空命名
     空间、命名空间外叶），每条都配有正例（四叶 × 两值 8 条 + 引号/注释 2 条）；
     另断言披露顺序（look → 四布尔 → parallel）。
- 矩阵：`git_d424_cohort_map()` + 分支置于 RFC 0039/0045 git 分支之前，
  post-loop 断言「到达集合 == 账本集合」（规则 38）。

## 4. 既有账本/断言迁移（本片足迹）

- **0045 门负例翻正**：`NamespaceBattery` 里 `parallelCommits: true` +
  `showBranches: false` 原本是「未知叶」负例，本片起被准入；按「只加不改口径」
  改为表外叶 `showTags: false`（仍整源回退），并注明 showBranches 的正负例归
  D-424 门所有。
- **E2a 整源扫描人口迁移**：18 行加入 `git_admitted` 跳过集，`checked` 由
  `EXPECT_EQ(checked, 80)` 重钉为 **62**（80 − 18）——正是效率规则 41 的
  「准入形状的阻塞检测器必须在同一提交退役」。
- roster `--emit`：**565 / 40 类**；`family_residual:git` **34 → 16**，
  其余 39 类计数零漂移；`--self-test` 与 `--matrix` 绿；E6c 冻结缺口 roster
  未动（`mandatory_gaps` 仍 48，受保护文件字节不变）。

## 5. 验证（收口实测）

- 定向：`MermaidGitBoolD424Cohort.*` 4/4；`MermaidGit0045Cohort.*` 5/5；
  `MermaidGitgraph0039Cohort.*` 5/5；E2a 5/5。
- 矩阵（debug）：`drawn_at_240` **1394**、`source_fallback` **565**、error 0、
  `mandatory_gaps` 48、`integrity_errors` 空、reach proof 通过。
  双预设逐字节相同：sha256
  `03e7f85191b11b1ff52604b05199abeef0a11faec69c12f3679cf62233a94660`
  （`matrix-d424-debug2.json` / `matrix-d424-release2.json`）；roster drift 门绿。
- `ctest -L mermaid`（run2，run1 因 E2a 未迁移而红）：**1519/1519 通过**，
  `CTEST_EXIT=0`，日志 `/tmp/loom-logs/ctest-mermaid-d424-run2.log`，
  退出码落盘 `/tmp/loom-logs/ctest-mermaid-d424-run2.log.exit`。
- **全量 debug ctest**：`100% tests passed out of 4704`，**`CTEST_EXIT=0`**
  （退出码落盘 `/tmp/loom-logs/ctest-full-dbg-d424.log.exit`，日志
  `/tmp/loom-logs/ctest-full-dbg-d424.log`，sha256 `2af6ed5f…`；4704 = 4699 + 5
  项新门）。无红项、无需隔离复跑。
- 三数并列：①绝对 source_fallback **565** ②超出认证 profile/冻结权威 **289**
  （未变：222 exact-bit + 67 受保护/冻结）③本轮净减少 **18**。

## 6. 未验证 / 剩余风险

- `rotateCommitLabel` 在终端**不实现旋转**，只披露；语料中的 2 行因此与 false 对
  画布相同——这是已声明的 inert 语义，不是遗漏。
- 18 行在 40 列窄视口全部回退（页脚超出视口），与既有 git cohort 一致（历史债
  narrow_40 全回退未动）。
- ASCII 渲染模式不解析 frontmatter，故这些行在 ASCII 下仍按无配置处理；矩阵只在
  宽 240 的 Unicode 路径计分。

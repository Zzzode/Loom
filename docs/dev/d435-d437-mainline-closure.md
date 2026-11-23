# 主线收口记录 —— D-435 / D-436 / D-437 之后（2026-11-23）

本文件是「按普查取下一族」这一轮的主线收口证据。全量 ctest 与双预设字节门
按要求只在收口跑一次，日志独立落盘（`/tmp/loom-logs/closure-d437-*`）。

## 1. 本轮净变化

| 片 | 提交 | 行数 | fallback | formal `drawn_at_240` |
|---|---|---|---|---|
| 起点（D-434 收口后） | `dae3a122` | — | 533 | 1426 |
| D-435 gitGraph frontmatter `title` | `72cc000a` | 1 | 532 | 1427 |
| D-436 gitGraph `mainBranchName`/`mainBranchOrder` | `2e9e2721` | 14 | 518 | 1441 |
| D-437 wardley 封闭 `config.theme` + column-0 体部 | `278e2ec2` | 4 | **514** | **1445** |

roster：**514 条 / 35 类**；`--self-test` 绿；E6c 48 条未动。

三数并列（沿用既有口径）：①绝对 source_fallback **514**；②超出认证
profile/冻结权威 **289**（未变；其内部拆解在 D-413 之前的文档里没有可复核推导，
本文件不重复引用）；③本轮净减少 **19**（533 → 514）。

## 2. 收口时发现并修复的跨门禁回归（D-436 收口债）

全量 ctest 首跑暴露 **2 条失败**，都是迭代期定向用例覆盖不到的地方：

1. `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource`：E2a 的
   「config/init 行必须保持整源」扫描没有登记 D-436 的 14 条 trunk 行，导致这
   14 行被误判为「意外改变画面」。修复：把 `git-mainbranch-d436-cohort.json`
   并入该门禁的 `git_admitted` 跳过集，`checked` 由 62 收紧为 **48**
   （提交 `0d338349`）。这正是 D-435 退休 E2a 检测器、D-436 扩大账本时应同步
   完成而当时遗漏的负向迁移。
2. `MermaidCapabilityReport.OfficialMatrix` 首跑的「Cannot read
   sankey-config-d438-cohort.json」是**构建树陈旧**：release 构建启动时
   D-438 探针分支还在 `test_mermaid_official.cpp` 里，revert 后未重编。
   重编后该用例通过（D-438 已整体 revert，见
   `docs/dev/d438-sankey-config-nogo.md`）。

## 3. 全量 ctest（release 预设）

- 命令：`cmake --build --preset release -j8`（退出码 **0**）→
  `ctest --preset release -j8`（带 `LD_LIBRARY_PATH` 指向 Homebrew OpenSSL，
  缺它时全部用例会 `libssl.so.3` 加载失败）。
- 测试：**4753 条，100% passed，退出码 0**（
  `/tmp/loom-logs/closure-d437-release-ctest2.log{,.exit}`）。
  其中 2 条 `Disabled`（`FullscreenLayout.StickyPromptClickFiresCallback`、
  `.NewMessagesPillClickCallback`）、25 条 `Skipped`（既有环境原因），
  无 `***Failed`。
- `mermaid` 标签合计 1568 条用例、494.80 sec。
- 用例名核验（TL 要求核到名字，均在通过日志内）：D-435/436/437 三族门禁
  12 条（含 3 条 `GeneratorIsReproducible`）、
  `MermaidFallbackAttribution.RosterIsReproducibleAndTotal`、
  `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource`、
  `MermaidCapabilityReport.OfficialMatrix` 全部 `Passed`。

## 4. 双预设字节门

- release `mermaid-official-matrix.json` sha256
  `ce7696ae8f85aed2d57d9ec8e55c8c5db277f5382cc7316a797379a5c211b4db`
  （`drawn_at_240` 1445、`mandatory_gaps` 48、`integrity_errors` 空）。
- debug 全量重编（604/604 退出码 0）后单独跑
  `MermaidCapabilityReport.OfficialMatrix`，同一矩阵 sha256 **逐字节相同**。
- 结论：双预设字节门通过。

## 5. 需要 TL 裁决的岔路（截至本收口仍全部阻塞）

1. **共享派发 `classify_mermaid` 的体部首行谓词**（D-438 发现，**17 行**）：
   `family_residual:sankey` 的 17 条残余全部用裸 `sankey` 首行，而共享派发只认
   `sankey-beta`。补丁已就绪（封闭 `config.sankey` 命名空间 + 生成器 + 账本 +
   门禁 + 矩阵分支，11 行可降），见 `docs/dev/d438-sankey-config-nogo.md`，
   补丁 `/tmp/d438-sankey-config.patch`（对 `04a98f1c` 干净应用）。
2. **共享 `classify_mermaid` 的 frontmatter 闭合围栏**（**2 行**）：只把恰好
   `---` 当闭合行，`---      `（tree-view）与 `  ---`（requirement
   `with-a-title-default`）都不派发。任何放宽都会改变全家族派发语义。
3. **git 体部 `%%{init}%%` 披露契约**（**8 行**）：今天的 init 准入值是静默的，
   直接承认 `rotateCommitLabel`/`themeVariables` 会静默丢语义；需要先定
   「要不要给既有已画帧补披露脚注」（会改大量已画帧字节）。
4. **git >8 分支容量**（**4 行**）：分支泳道容量是几何特性，不是配置片。
5. **xychart 退化定义域/点标签**（**1 + 4 行**）：零宽 x 已证明能解析，但
   line-connector 绘制阶段仍整体拒绝；需要「单列只画标记、不画连线」的渲染决策。
6. **info 家族**（2 行）：需要作者化「版本/信息面板」的内容，属新渲染面。
7. **state 路由几何**（`state-native-routing-debt`，sha 绑定的评审集）、
   **timeline 体部 init 嵌套映射**（2 行）、**katex/zenuml**（外来引擎）、
   **venn/quadrant/er 的 style 语句**（`classdef_styling`，debt）都需要各自
   的裁决或 RFC。

## 6. 本收口未改动的既有契约

- 十个受保护文件、ER107 冻结面、E6c 48 条、排除 43 条、flowchart exact-bit 222
  均未触碰。
- 除 `0d338349`（门禁负向迁移，测试侧）外，本轮收口没有生产代码改动；
  三片（D-435/436/437）各自为独立提交，可逐个 revert。

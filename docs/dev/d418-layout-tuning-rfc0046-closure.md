# D-418 —— RFC 0046 layout/display 调参命名空间收口记录

日期 2026-11-22 · 本地，未 push · 承接 TL 常驻授权（RFC 0045 验收通过后继续下一片，
家族由 D-417 roster 实测选定）。受保护 43 条、flowchart exact-bit 222 条与三笔历史债
未动。

## 1. 结论（三数并列，TL 口径）

| 指标 | 值 |
|---|---|
| ① 绝对 `source_fallback` 总数 | **617**（628 → 617） |
| ② 超出认证 profile / 冻结权威的部分 | **289**（未变：222 exact-bit + 67 受保护/冻结） |
| ③ 本轮净减少 | **11** |

全局 **1517/628 → 1528/617**；formal `drawn_at_240` **1331 → 1342**；
`mandatory_gaps` 48、`integrity_errors` 空；roster 628 → **617**，类别 **43 → 41**
（`family_residual:ishikawa` 与 `family_residual:architecture` 清空）。
家族：ishikawa **9/4 → 12/1**、architecture **16/4 → 20/0**、state v1 **28/14 → 30/12**、
state v2 **52/51 → 54/49**。

## 2. 机制

- 三个渲染器 TU 各扩展自己的 frontmatter 门，形状一致：一个 indent-2 命名空间 +
  indent-4 闭合值叶，其余全部 fail-closed。
  - ishikawa（行扫描）：`ishikawa.useMaxWidth {true,false}`、`diagramPadding {50}`；
    与 RFC 0043 的 `theme` 叶**互斥**。
  - architecture（原平铺扫描改为结构化）：`architecture.randomize {true,false}`、
    `seed {42}`、`idealEdgeLengthMultiplier {3}`；仅字面 `config:` 开块；新增
    `arch_scalar`/页脚通道。
  - state（`state_source_envelope`，`parse_envelope` 未动）：`state.useMaxWidth
    {true,false}`，命名空间无叶显式拒绝。
- 页脚按家族规范顺序发射（与源码顺序无关），最长行 77 字节 < 96。
- twin：2 条 corpus（ishikawa `pr-check/1`、architecture `default-fcose-knobs`），
  9 条 synthetic；synthetic 的 config-free 判定是结构化的（描述正文含键名，不做
  子串搜索）。
- RFC 0043 smallfam 契约保持：ishikawa `admitted`（2 条 theme）与 digest 不变，
  3 条 tuning 行进独立 `layout_admitted` 桶（独立 digest），分区五路
  `13 = 7 + 2 + 3 + 1 + 0`。

## 3. 既有账本/测试同步（本片足迹）

- architecture：ledger 16/4/0 → 20/0/0（`draw_count`、`counts.*` 同步），生成器
  `gate_kind` 改为结构化镜像；测试计数更新、`GatedArchitecturesStayWholeSource`
  改为显式空断言、`VisualGateIsKeyBased` 正例翻转 + 超表负例，并新增 16 条
  clean 行的**帧恒等断言**（whole == body 且无页脚 / 调优行有页脚与
  `config.architecture.` 行）。
- ishikawa：ledger 4/2 → 1/2 + `layout_admitted` 3；测试计数与 layout 桶断言。
- smallfam 0043：FamilySpec 新增 `expected_layout_admitted`（3/0/0），分区与
  disjoint 覆盖循环五路；`ishikawa.useMaxWidth` 负例按 spec 翻转（venn/railroad
  保持拒绝）并配对 `useMaxWidth: maybe`。smallfam ledger 与生成器不变。
- state 0038：`state.useMaxWidth` 负例翻转并断言页脚行，配对
  `useMaxWidth: maybe` / `state.width` / 空命名空间；生成器仅改文案，
  `EXCLUDED_EXPECTED["namespaced config.state.*"]` 仍为 4。
- E2a：加入 0046 账本，git 集合相等保留，新增 `state_skipped == state_admitted`
  （108 → 104 行）。E2b：`checked` 43 → 39，新增 `skipped_by_rfc0046` 集合相等。
- 矩阵：0046 分支置于所有家族分支**之前**（只匹配自身 11 条路径），
  `raw_active_schema` 留空。

## 4. 验证（收口实测）

- 定向：`MermaidLayouttuning0046Cohort.*` 6/6（5 gtest + 1 hermetic `--check`）；
  architecture 7/7、ishikawa 7/7、smallfam 6/6、state-0038 5/5、E2a 5/5、E2b 6/6。
- **双预设矩阵逐字节相同**：debug/release 报告 sha256 均为
  `10e819f807e93df062f59a8b38938b911b03e0d6e6329634861ca6d6be621093`。
  实测 `original ALL wide_240` **1528 drawn / 617 fallback / 0 error**；
  `drawn_at_240` **1342**（2145 分母）；gaps 48；integrity 空。
- roster `--emit` **617 / 41 类**；`--self-test`（617/41/零未分类/可复现）与
  `--matrix`（617 行全匹配）绿。
- **全量 debug ctest**（TL 2026-11-22 流程要求：完整 stdout + 退出码落盘）：
  首跑 `/tmp/loom-logs/ctest-full-dbg-0046.log`（1143375 B，sha256
  `a64fe72be2e02bf0b0bb89ad1f4b6109bb0f2b4812ffa5f44a990a7a94457abc`）为
  `99% tests passed, 2 tests failed out of 4687`、`CTEST_EXIT=8`；两条红项是
  AGENTS.md 载明的 `ServerMain.DirectConnect*` TCP 时序 flake（负载 10.8/28.3/32.3），
  隔离下各 3 次干净重跑全通过。复跑日志见 §6。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退（本片 11 行亦全部 narrow 回退，页脚超视口；
architecture 两条 body 轨在 40 列可绘，已按实测记入账本）、ER107 B3 三门未动。

## 6. 评审与效率

- **全量 ctest 复跑**：`/tmp/loom-logs/ctest-full-dbg-0046-retry.log`（sha256
  `b5aed5405a3076233d32ebd067ce4d0ea9bb909ced97761afc7a1fa994695dc6`）为
  `99% / 3 failed out of 4687`，红项换成另外三条计时敏感测试
  （`McpStdio.WatchdogKillsSilentChild`、`Tools.RuntimeTeamCreate…`、
  `ReplScreen.VirtualAssistantSpacing…`），各自在隔离下 3 次干净重跑全通过；两轮
  全量红项互不相同，且本片改动只触及 mermaid 渲染 TU 与 mermaid 账本/测试，无
  MCP/工具运行/REPL 路径——负载驱动（首轮与复跑期间 load average 10–32）而非回归。
  第三次全量运行 `/tmp/loom-logs/ctest-full-dbg-0046-run3.log`（sha256
  `4cda1d444326ec84ec78c403415bf190dc7ad7cee49c0628e2793deb322ae4f8`）为
  **`100% tests passed out of 4687`、`CTEST_EXIT=0`**，即本片最终证据为一次全绿
  的完整运行。

- 代码评审 r1 REQUEST-CHANGES → r2 **APPROVE**：r1 的实质问题（0046 矩阵分支被
  architecture 分支遮蔽 4/11 行、ishikawa 门与镜像在 theme+namespace 互斥上不一致、
  architecture 镜像跳过 description payload 行、裸 `config` 无冒号仍开块）全部修复；
  两个镜像边缘 NIT 也已对齐（payload 裸键行、隔离的裸 config 负例）。
- 效率复盘：矩阵分支位置必须按“最具体者先”复核——同一家族的旧分支会静默吞掉新
  账本（本轮 M1）；镜像与门的分歧要在同一轮用“正例 + 负例”钉住，而不是依赖 corpus
  不触发。

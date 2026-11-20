# ER107 live matrix join 重启可行性判定 + RFC 0033 总目标缺口清单（D-392）

日期 2026-11-20 · 分支 master · 起点 HEAD `3a90551c` · 本地执行，未 push、未 merge、零生产代码改动（本里程碑仅分析 + docs-only 入库）。

## 1. 判定结论（先行）

`MATRIX_API_CONFIRMED=False` 的卡点是**内部的、主动设置的 fail-closed 评审/确认门，不是外部 API 依赖**：不涉及任何外部团队、网络服务或待交付物；所谓 “Main must confirm this integration API” 中的 API 是本仓库 C++ Official worker（`tests/ui/test_mermaid_official.cpp`）写出的 matrix JSON 合约形状，“Main” 是主线 agent 评审权威（Block58 先例的 B3 评审由 Harvey agent 执行）。

该 join **原则上内部可自解**，但当前授权下不能直接开工，因为重启路径必然：(a) 翻转一个十保护文件内的 B3 状态常量；(b) 变更已 pin 的验证合约与冻结摘要（仓库规则要求先立新 RFC 并过 agent 评审）。本里程碑的合法交付物因此是：可行性判定、机械漂移清单、可执行解除条件（本文档），而非抢先实现。

## 2. 三个 fail-closed 门（全部内部评审门，代码实证）

| # | 位置 | 现状 | 等待的输入 |
|---|---|---|---|
| 1 | `tools/mermaid/verify_er107_admission.py:346` `MATRIX_API_CONFIRMED=False` | `matrix_check()`（:355）不可达，真实矩阵 join 校验不运行 | Main 确认 matrix summary/provenance 集成 API 形状 |
| 2 | 同文件 `:66` `REVIEWED_HELPER_SHA={}` + `tests/support/mermaid_er107_authority.hpp:2010` `kB3ReviewStatus="UNREVIEWED"` | `authority_gate()`（:221-226）拒绝：四 helper 无 B3 评审收据，且状态常量未批准 | 对四个 helper 的独立 B3 评审 + 保护文件内常量翻转 |
| 3 | 同文件 `:842` `WRITE_REVIEW_APPROVED=False` | `--write` 发布 join marker 被拒（:855） | join 实现的独立评审 |

注释明确 “No flag/env override may invent missing matrix evidence”（:345），自检 `test_unapproved_helper_and_pending_matrix_api_fail_closed`（:739-746）把两个门的 fail-closed 行为锁成测试。今日复跑 `--self-test`：**15/15 OK**。

### 2.1 四个待评审 helper 全部是十保护文件

`HELPERS`（:62-63）= `mermaid_er107_source.hpp`、`mermaid_er107_authority.hpp`、`mermaid_er107_official.hpp`、`mermaid_relations_oracle.hpp`——四个都在字节不可变的十保护清单内。门 2 的收据填写本身改的是 verifier（非保护），但 `kB3ReviewStatus` 翻转改的是保护文件，超出当前里程碑授权。

## 3. worker 与 verifier 的现状：侧车已写、join 未发

新鲜普查矩阵（sha `ee743515…`）实测：

- 顶层 `ER107_native_whole_source` summary 存在；107 个选中行**逐行携带** provenance 块（worker 发射点 `test_mermaid_official.cpp:8325` 行块、`:8589` summary）。
- 107 行 wide_240 **全部 drawn、零 errors**；narrow_40 = 70 drawn / 37 source_fallback（按 `narrow_exact_ordinary` 分流）；summary `admitted=0, join_required=true, base_admitted_originals=688`。
- join marker `ER107_dual_consumer_join` **不存在**；全矩阵没有任何 `*_dual_consumer_join` / `*_admission` marker。

## 4. 合约形状漂移的机械清单（live 对 verifier 冻结目标）

| 面 | verifier 冻结合约 | live 实际 | 性质 |
|---|---|---|---|
| summary 键集 | 闭集 `SUMMARY_KEYS`（10 键，:347-348） | 缺 `terminal_facts_sha256`（live 改名 `projection_facts_sha256`）；多 11 个键（`contract`/`*_sha256` 族/`drawing_credit`/`formal_admission`/聚合断言等）；`prerequisite_join` 已在（值 `"Block58_native_whole_source"`） | 命名演化 + worker 侧更富闭集，需裁决谁向谁对齐 |
| 行块键集 | 闭集 `ROW_KEYS`（11 键，:349-350） | **11/11 完全一致** | 无漂移 |
| 行块 `original_foreign_obligations` | 结构化对象 `{status: retained_not_certified, …}`（:388-390） | 字符串（"Retained in frozen ER206, …"） | 内容形状漂移 |
| 行块 `actual_color_cells_file` | `mermaid-official-er107.jsonl`（:383） | `mermaid-official-er107-matrix.jsonl` | 文件名漂移 |
| 矩阵基线摘要 | `BASE=688`、`FINAL=795`、formal `drawn_at_240=688`（:36, :371-372） | formal **1053**、observed wide drawn **1239** | 台账代际陈旧 |
| 三个冻结 digest | `BASELINE_MATRIX_SHA=ef0f12e6…`、`RETAINED_ROWS_SHA=08b9a546…`、`PRIOR_SELECTED_DEBTS_SHA=251ac00f…`（:60-61, :351） | 当前矩阵 sha `ee743515…`；RFC0033 六行 + RFC0034 一行自冻结后改判，retained 集必然变化 | 必须重算重钉 |

漂移方向已用 `git log -S` 确认：两个内容漂移都源自 verifier 出生提交 `cf34725a`（2026-10-28）——worker 与 dormant verifier 自入库起就是“当前侧车形状 vs 目标合约形状”并存，verifier 刻意休眠等待 API 确认，不是后续回归。

## 5. Block58 成功先例与三级前置链（同构路径，但范围比单点更大）

`verify_block58_admission.py` 走通过评审路径：B1 源 → B2 设计/PRR → **B3 独立双消费者评审** → helper 中 B3 常量达到 `B3_REVIEW_APPROVED`（实测 `tests/support/mermaid_block58_authority.hpp:20` 已是批准值，对照 ER107 的 `UNREVIEWED`）。ER107 的机制与它同构，且自身 machinery 已完备：`matrix_fixture`（:748-756）用合成矩阵 + 临时 patch digest 证明 `matrix_check` 端到端可用，缺的只是真实收据、合约对齐与重定基线。

但实测发现重启范围不止 ER107 一个点，存在**三级验证器前置链**：`verify_er107_admission.py` import `verify_block58_admission.py`，后者 import `verify_shapes4_admission.py`（Shapes4 又内嵌 Mindmap10 旧门）。ER107 的 `baseline_prerequisite`（:329-335）要求矩阵中先存在 Block58 join marker，Block58 的 `legacy_prerequisite`（:341）又要求 Shapes4/Mindmap10 marker 在先。当前 live 矩阵三者全部缺席。

两个必须如实记录的稳态事实：

- **unjoined 是这些程序的设计稳态，不是故障**。Block58 自己的 `matrix_check` 显式断言 `JOIN not in matrix`（:381）与 `admitted=0`（:385）；它们的合约本就是“源证据零绘制信用”，侧车 provenance 即交付物。
- **它们的完整矩阵路径已台账代际陈旧**。Block58 `matrix_check` 仍要求 formal `drawn_at_240=630`（:376，630/688 时代），对当前 1053 的 live 矩阵必然不通过；四个验证器进 ctest 的都只有 `--self-test` 合成用例（`tests/ui/CMakeLists.txt:193/199/215/222`），完整只读路径是按需 CLI，非常驻门。因此 ER107 重启 = 把整条链的 marker 接入矩阵并把三代陈旧基线一起重钉，这正是其工作量被评为中-大的结构性原因。

## 6. 重启工作分解（每项标注触碰面与门）

| 项 | 工作 | 触碰面 | 门 |
|---|---|---|---|
| A | 立新 RFC：裁决 §4 合约形状对齐方向、冻结 digest 重算方案、RFC0033/0034 七行在 retained/selected 集中的处置，**以及三级前置链（Shapes4/Mindmap10 → Block58 → ER107）的 marker 接入顺序与各自陈旧基线（630/688 时代）重钉方案** | docs + verifier（非保护） | **RFC 评审门（前置）** |
| B | 独立 B3 评审四个 ER107 helper，通过后 `kB3ReviewStatus` 翻 `B3_REVIEW_APPROVED` | **保护文件** authority.hpp | **保护文件授权 + agent 评审** |
| C | verifier 填 `REVIEWED_HELPER_SHA` 四 hash（实测值） | verifier（非保护） | B 完成后 |
| D | 按 RFC 裁决做合约对齐（worker 侧车或 verifier 闭集，二选一，均为 test-only） | test_mermaid_official.cpp / verifier | RFC 内实现评审 |
| E | 重算并替换 ER107 三个 digest、`BASE/FINAL` 计数（新预期 formal = 1053+107 = **1160**，observed 1239 不变），同步重钉链上 Block58/Shapes4 的 630/688 时代基线 | verifier 族 | RFC 内，全矩阵双 preset 字节门 |
| F | 按前置链顺序发 marker，翻 `MATRIX_API_CONFIRMED=True`、`WRITE_REVIEW_APPROVED=True`，`--write` 发布，单收口全矩阵 | verifier + 矩阵产物 | 两个独立评审收据 |

全程零 `src/` 生产改动（worker 在 tests/，verifier 在 tools/）。规模中-大、多会话，与 `0033-slice2-closure.md §8.2` 的预估一致。

## 7. 解除条件（外部阻塞不存在；这是内部授权/评审清单）

1. TechLead 授权开新 RFC（并授权 B 阶段对 `mermaid_er107_authority.hpp` 做一行常量翻转，这是十保护文件的唯一必改点）。
2. RFC 对 §4 五项漂移逐一作出合约裁决并过 agent-run 评审。
3. 新鲜上下文 agent 完成四 helper B3 评审，给出 sha 收据（填入 C）。
4. join 实现独立评审通过后翻 F 的两个开关；按 slice 纪律先写/调验收用例（`matrix_check` 的真实矩阵用例，替代今日仅合成数据的自检），再实现。
5. 按 Shapes4/Mindmap10 → Block58 → ER107 顺序接入 marker 并重钉链上 630/688 时代陈旧基线；双 preset OfficialMatrix 字节一致、formal 1053→1160、observed 1239/906/0/48 不退化、十保护文件仅 B 那一行为授权变更。

## 8. 本里程碑为何不提交任何“准备代码”

TL 条款 b 的可交付准备以“外部依赖”为前提；本项判定为内部，且每条可能的代码路径都撞门：改 helper 触保护文件；改 verifier 合约/ digest 是未授权的验证契约变更；在 RFC 裁决形状前写漂移检查器属于推测性工具，目标形状一旦反向裁决即返工。故按 Occam/范围纪律，只交付判定与解除条件；RFC 草稿可在授权后立即开工。

---

## 9. RFC 0033 距总目标还差什么（D-392 任务 3，非复述既有文档）

### 9.1 RFC 0033 自身问题已闭合，且无未登记的同类 whole-null 缺口

RFC 0033 只问六个 whole-null group-edge 整源的权威证明，已闭环。对 ER 全量 206 个 whole source 的 `whole_null_pending=138` 做去向对账，**138 行全部有名有姓，不存在未登记的同情形缺口**：

```
138 whole-null pending = 107 ER107 选中行（§8.2 休眠 join）
                       +  6 RFC 0033 group-edge（已 owned-proof drawn）
                       +  1 RFC 0034 ordinal-198（已 drawn）
                       + 24 冻结 source_fallback（138−114=24，实测 ER fallback 行数恰为 24）
```

24 个残余行的 fixture 级分组：class/style 语句类 15（roster 类别 `classdef_styling`，causality=`debt`，自有未实现面）、unicode/htmllabels 类 8（`htmllabels_false`/`look_handdrawn`，causality=`foreign-engine`，依赖外部引擎行为）、多行关系标签 1（`multiline_label`，`debt`）；另有 1 行归 `family_residual:er-diagram`（`family-scoped-debt`）。它们与五个终态 SourceProfile literal 暴露的是同一批已知未实现面（classDef/style/accessibility/kind-collision），受冻结 roster 锁定，不是新缺口。

### 9.2 observed 1239 vs formal 1053 的 186 行差额——精确闭合

按行 `expected` 字段机械归因（可复算）：

| 组成 | 行数 | 性质 | 处置状态 |
|---|---|---|---|
| `expected=pending` 的 ER 补充绘制 | 114 = 107 ER107 + 6 RFC0033 + 1 RFC0034 | 全部在 er-diagram；supplemental 槽位按设计不给 formal 信用 | 107 待 §8.2 join；7 为 RFC 既定终态 |
| `expected=drawn` 但程序合约 `admitted=0` 的零信用绘制 | 72 = Block58 58 + Mindmap10 10 + MindmapShapes4 4 | 各自合约 `formal_admission=false/drawing_credit=0`（源证据非绘制信用） | 见 §9.3 |
| 合计 | **186** | 1125 `expected=drawn` − 72 = **1053**，与 formal 指标逐字节对账成功 | — |

### 9.3 §8.2 之外实际存在的关联缺口：72 行零信用与三级前置链（本文新登记）

这是既有文档未登记的发现，需区分两个层面：

- **合约层面是设计稳态，不是故障**：Block58（58）、Mindmap10（10）、MindmapShapes4（4）与 ER107（107）同样在 live 矩阵中 `join_required=true / admitted=0`、无 `*_dual_consumer_join` marker；但 Block58 自己的 verifier 把 `JOIN not in matrix` 作为断言（`verify_block58_admission.py:381`），即“源证据零绘制信用、侧车即交付物”本就是这些程序的批准终态。它们不是待修的 bug。
- **重启层面是 ER107 的隐式前置债务**：三个验证器构成 Shapes4/Mindmap10 → Block58 → ER107 import 前置链，ER107 的 join 设计要求链上 marker 先入矩阵，而链上 verifier 的完整矩阵路径仍钉着 630/688 时代 formal 基线（与 live 1053 已不符，仅合成自检常驻 ctest）。因此 ER107 重启不是单点翻转，而是整条链的 marker 接入 + 三代基线重钉。建议单立一个“formal admission 重启”RFC 族统一排序，不要在各 verifier 内分别悄悄翻开关。

### 9.4 面向“零 fallback”北极星的其余缺口（RFC 0033 范围外，已登记且冻结）

- 整源 wide fallback **906 行 / 44 类**（`tools/mermaid/fallback_roster.json`，causality 全分类、零未分类）：头部为 `look_neo` 160、`edge_animation_metadata` 91、`look_handdrawn` 70、`htmllabels_false` 52、`family_residual:git` 46、`elk_layout_engine` 38、`classdef_styling` 37、`xychart` 35、`securitylevel_directive` 34、`subgraph_nesting` 31；跨 33 个图族，flowchart 240 / requirement 156 / git 77 为重灾区。
- 强制 gap **48**（`e6c-mandatory-gap-roster.json`，sha/字节冻结）。
- body_diagnostic 轨道：173 fallback + 48 error（48 是设计内错误处理用例，非缺陷），与本里程碑复证结果一致。
- 这些都是终端能力/外部引擎依赖的已知终态或排队项，受 drift gate 保护；任何缩减都应走既有“RFC + owned-proof + 冻结 roster 迁移”模式，无捷径。

### 9.5 总缺口清单（一张表）

| 缺口 | 规模 | 登记状态 | 解除路径 |
|---|---|---|---|
| ER107 休眠 join（§8.2） | 107 行 formal 信用 | 已登记，本文给出解除条件 | 新 RFC + B3 评审 + 保护文件一行授权 + 三级前置链重钉 |
| Block58 / Mindmap10 / MindmapShapes4 零信用 | 72 行 | **本文新登记**：设计稳态（unjoined 为断言终态），同时是 ER107 重启的前置链 | 同族 RFC 统一排序，随 ER107 marker 接入 |
| RFC0033/0034 supplemental 7 行 | 7 行 pending | 既定终态（设计上不进 formal） | 无需动作 |
| ER 残余 24 fallback | 24 行 | roster 已分类（debt/foreign-engine） | 新能力 RFC（classDef/htmllabels/handdrawn 等） |
| 全族 fallback | 906 行 / 44 类 | 冻结 roster + drift gate | 逐族 owned-proof 程序 |
| 强制 gap | 48 | 字节冻结 | 能力突破后走 roster 迁移 |
| body track | 173 fallback / 48 设计 error | 已登记 | 随整源程序推进 |

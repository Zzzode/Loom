# RFC 0033 slice 2 回归复验与剩余 slice 清单（D-391 任务 2）

日期 2026-11-20 · 分支 master · 复验起点 HEAD `492937d9` · 本地执行，未 push、未 merge、零生产代码改动（本任务无代码 diff，仅证据复验 + 本文档入库）。

## 1. 结论（先行）

RFC 0033 的唯一在册问题——六个 whole-null ER group-edge 整源如何获得权威 owned proof——已由 slice 2 的六个阶段完整回答并度量收口；**slice 3 / slice 4 在整个仓库中从未被定义**，收口评审留下的两个 NIT 也已于 2026-11-18 作为常规测试维护独立落地。当前不存在可在不立新 RFC 的前提下开工的 RFC 0033 实现工作；唯一在册的后续候选（ER107 live matrix join 重启）变更验证契约，按 RFC 驱动规则须先立新 RFC，不在本里程碑授权范围内。

## 2. Ground truth：slice 2 工作项与落地位置

| 项 | 状态 | 落地 commit / 证据 |
|---|---|---|
| Slice 1 解析器准入 + §7.0 全帧 oracle | 已落地 | `426fcb58` |
| Slice 2 六阶段（proposal、双 support header、verifier、matrix 接线、smoke、T1 转正、收口矩阵） | 已落地，HEAD `ee417aac` | `docs/rfcs/tracking/0033-slice2-closure.md`（收口文档 `fc642506`） |
| 评审 NIT-1：两个转正 literal 缺直接 seam 对等断言 | **已落地** | `62580e0c`（2026-11-18，test-only，+81 行）：新增 `MermaidErSubgraphSourceProfile.WholeSceneSeamDirectlyMatchesAllSixteenLiterals`，对全部 16 个 literal 直接钉住 `er_whole_scene_json_for_testing` 输出（7 positive 规范化 canonical 对等、9 closed 必须 nullopt） |
| 评审 NIT-2：报告计数硬编码（7+5+4=16） | **已落地** | `1e61d60a`（2026-11-18，test-only）：`er_subgraph_profile_count()` 从 `kErSubgraphLiterals` 表派生三个 profile 计数，附 partition `static_assert` |
| §8.2 候选 ordinal 198（handDrawn Work 上限行） | 已由独立 RFC 闭环 | RFC 0034 全四阶段完成（本仓库 HEAD 序列内，普查 ordinal 198 已 drawn） |
| §8.2 候选 ER107 live matrix join 重启（`MATRIX_API_CONFIRMED=False`） | **未启动，唯一在册剩余项** | 变更验证契约，须新 RFC；见 §4 |

NIT-1/NIT-2 均按收口文档 §8.3 指定路径执行：普通测试维护、独立 commit、无需 RFC；两者都是单文件 `tests/ui/test_mermaid_relations_semantics.cpp` 的 test-only 改动，不触 `src/`、不触十保护文件。

### Slice 3 / 4 不存在的复核

`docs/rfcs/tracking/0033-slice2-closure.md` §8.1 的结论今日仍然成立：实现规格标题虽含 “Slice 2/3/4”，但正文是单个六阶段实现；父 RFC（`0033-er-group-edge-owned-proof.md`）中 “slice” 仅指 witness 字节跨度（`source_slice_sha256`），从非工作包划分；全树 `git grep` 无任何 slice 3/4 的目标、边界或前置定义。没有可“开工”的对象。

## 3. 今日复验证据（针对性门，未放宽任何断言）

构建：Homebrew LLVM 23.1.2，debug preset，两个目标增量构建均为最新（`ninja: no work to do`）。

1. **子图 profile 全套件 4/4 PASS**（`test_mermaid_relations_semantics --gtest_filter='MermaidErSubgraphSourceProfile.*'`，458ms）：
   - `CompleteOwnershipTitlesTypedMembersAndRecovery`（NIT-2 派生计数的宿主，16 literal 全量）OK；
   - `Rfc0033GroupEdgeFullFrameOracleAcceptsSix`（§7.0 六例 oracle）OK；
   - `WholeSceneSeamDirectlyMatchesAllSixteenLiterals`（**NIT-1 验收测试**，7 positive / 9 closed 计数断言）OK；
   - `Rfc0034WholeHandDrawnSourceDrawsAtProductionWorkLimit` OK。
2. **RFC 0033 注册门 60/60 PASS**（`ctest -R "GroupEdge|FallbackAttribution|Er107"`，257.34s，mermaid label），含三个锚点测试 `MermaidEr107AdmissionJoin.*`、`MermaidErGroupEdgeAdmission.RejectsUnreviewedOrDriftedCohort`、`MermaidFallbackAttribution.RosterIsReproducibleAndTotal`。
3. **只读验证器逐字输出 rc=0**：
   ```text
   Read-only verified ER group-edge cohort: six owned proofs, four-part cohort
   assertion, RFC 0034 ER198 work-ceiling retirement assertion, ten protected
   files byte-identical; both helper shas match the independent review receipt.
   ```
   十保护文件 sha/byte 在本次复验中再次机械比对通过。
4. **cohort smoke 5/5 PASS**（`test_mermaid_er_group_edge_cohort`，4.8s）。
5. **普查侧对账**（与任务 1 同一次新鲜 OfficialMatrix，sha `ee743515…`）：六个 RFC 0033 group-edge fixture 在 wide_240/restored_240 全部 drawn，narrow_40 维持写死的 3 exact-ordinary / 3 full-geometry 处置；零 error、mandatory_gaps 冻结 48。

## 4. 剩余 slice 清单（对照 RFC 0033 总目标）

- **RFC 0033 总目标**：六个 whole-null group-edge 整源的权威 owned proof 路径——已回答、已度量（收口时 wide drawn 1238/907 fallback/0 error/48 gap；经 RFC 0034 后当前 1239/906/0/48，增量全部可归因，见 `docs/dev/mermaid-zero-fallback-recertification.md`）。
- **Slice 1、Slice 2**：完成，无遗留。
- **Slice 3 / Slice 4**：不存在定义，无剩余工作可言。
- **评审 NIT-1 / NIT-2**：完成（`62580e0c`、`1e61d60a`），今日复验绿色。
- **唯一在册后续候选**：重启休眠的 ER107 live matrix join（`tools/mermaid/verify_er107_admission.py` 中 `MATRIX_API_CONFIRMED = False`，fail-closed 保留）。前置 =  verifier 契约与留存 688 台账对当前 1239 普查的重对账；性质 = 变更验证契约，按仓库规则**必须先立新 RFC 并过 agent 评审**，本任务未授权、未启动。

## 5. 边界与全量门说明

- 本任务零代码 diff：NIT-1/NIT-2 早已在树中，今日只做证据复验；新增物仅本文档（docs-only）。
- 因代码树相对 D-390 全量零失败收口（`555f89c1`，4606 = 4581 通过 + 25 运行期 SKIP + 0 失败 + 2 编译期 DISABLED）只多了一个 markdown docs commit，被测代码字节相同，重复全量 ctest 不产生新信息；本任务按工程效率规则以针对性门 + 任务 1 的 246s 全量 OfficialMatrix 作为收口证据。十保护文件经验证器逐字确认 byte-identical，无 src/、wire、持久化或契约变更。

## 6. RFC 0033 距总目标还差什么（D-392 任务 3 追加）

完整证据与机械对账见 `docs/dev/er107-join-reactivation-feasibility-d392.md` §9；摘要：

1. **RFC 0033 自身范围已完全闭合，无未登记的同类 whole-null 缺口。** ER 全部 138 个 whole-null pending 行去向可闭合：107 ER107（休眠 join）+ 6 RFC 0033 + 1 RFC 0034（均已 drawn）+ 24 冻结 fallback（138−114=24，实测行数吻合），24 行的 blocker 在 fallback roster 中全部有名分类。
2. **observed 1239 与 formal 1053 的 186 行差额精确对账**：114 个 ER `expected=pending` 补充绘制（107+7）+ 72 个程序合约零信用绘制（Block58 58 + Mindmap10 10 + MindmapShapes4 4，均 `admitted=0/join_required=true`）；1125−72=1053 与 formal 指标闭合。
3. **§8.2 之外新登记的关联缺口**：Block58（58）/Mindmap10（10）/MindmapShapes4（4）的 72 行零信用绘制——unjoined 是其 verifier 断言的设计稳态而非故障，但三个 verifier 构成 Shapes4/Mindmap10 → Block58 → ER107 前置链，链上完整矩阵路径仍钉 630/688 时代基线，是 ER107 重启的隐式前置债务；建议纳入同一个 formal-admission 重启 RFC 族统一排序。
4. **范围外的已知冻结缺口**：906 整源 fallback（44 类，roster 锁定）、48 强制 gap、body 轨道 173 fallback + 48 设计 error；无捷径，逐族走 owned-proof 程序。

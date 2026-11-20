# D-395 —— F0 inert-key 放宽收口记录

日期 2026-11-21 · 分支 master · 起点 HEAD `2b4443c2` · 本地执行，未 push、未 merge。
范围：D-066 F0（inert-key 值放宽，计划 §3.0 明确 RFC-exempt）+ xychart RFC 草案 + ER107 台账代际刷新；NIT-1/NIT-2 为既有落地项，本轮复验。

## 1. F0 实现（requirement 族，8 行晋升）

requirement 信封此前整体拒绝任何 `config` 子树。本轮按 D-066 F0 的内联白名单模式（对齐 state E2b + flow 先例）放宽为：description 可与仅含**非终止（inert）叶子**的 `config` 共存——`htmlLabels`（bool）、`logLevel` 0/1/2、`fontFamily: courier`；其余一切 `visual/semantic` 键、命名空间键、非法值、重复键、非法缩进、tab、CRLF、空 config 均继续整体回退。接受的非终止叶子通过有界 footer 披露（`Requirement native projection v1` + 每叶一行），与 state E2b 同格式。

- 落地：`src/ui/visual/mermaid_requirement_impl.cpp`（白名单 + footer 组装 + Work 计量含字节）、`mermaid_detail.cppm`（`RequirementEnvelope.config_notices`、`RelationDiagram.requirement_config_notices`）、`mermaid_relations_core_impl.cpp`（footer 通道泛化到 requirement，state 行为不变）。
- 晋升机制：代码侧受评审 cohort ledger（journey/timeline 先例，**不动 manifest 及其 sha 级联**）：`tools/mermaid/requirement-f0-cohort.json` 列 8 行 + 各自 config-free `-default` 孪生 + 三态；矩阵分支按既有模式记录声明态；逐行帧门在 `tests/ui/test_mermaid_requirement_cohort.cpp`（经公共 `render_markdown` 渲染，断言：原生绘制、帧 = 孪生帧 + 精确 footer、三态、确定性与 16 类 fail-closed 边界）。
- **过程中发现并修复的真实缺陷**：首版顺手准入嵌套 `config.flowchart.htmlLabels`，使被推迟的 `edge-labels...-flowchart-htmllabels...` 行开始绘制但无 cohort 覆盖 → 普查出现 1 error、gaps 48→49。按范围纪律把门收窄为仅顶层叶子，该行保持 source 且被边界测试显式钉住；此后普查干净。
- **推迟项**：`edge-labels-correctly-when-flowchart-htmllabels-is-false-default.mmd`（无 config-free 孪生，无法用帧等价证明 inertness），继续 whole-source；理由写入 ledger `selection.deferred`。

## 2. 普查前后对比（写死口径）

| 指标 | 前（`ee743515`） | 后（`008d5ca1`） | Δ |
|---|---|---|---|
| wide_240 drawn | 1239 | **1247** | +8 |
| wide_240 source_fallback | 906 | **898** | −8 |
| wide_240 error | 0 | **0** | 0 |
| formal `drawn_at_240` | 1053 | **1061** | +8 |
| mandatory_gaps | 48 | **48**（修复后） | 0 |
| integrity_errors | 0 | 0 | 0 |
| narrow_40 drawn | 634 | 634 | 0（8 行窄屏仍回退，ledger 如实记录） |
| requirement 族 wide | 16 drawn / 156 fallback | **24 / 148** | +8 / −8 |

双预设字节门：debug 与 release 的 `mermaid-official-matrix.json` sha 均为 `008d5ca1fd23ddbb4913cd9f329d64cb404e797427ae8440399ede2a31ca32b5`（debug 296.1s / release 27.3s）。

## 3. Fallback roster 迁移（仅经工具）

8 行晋升后 `fallback_attribution.py --matrix` 按预期红色（live 898 vs pinned 906）。用工具重生成，未手改：
`python3 -I tools/mermaid/fallback_attribution.py --emit build/debug/tests/ui/mermaid-official-matrix.json -o tools/mermaid/fallback_roster.json`
→ 898 行 / 44 类；`--matrix` 漂移门绿（"898 fallback rows match the roster; no unregistered or reclassified fallback"）；`--self-test` 绿。`htmllabels_false` 52→44；被推迟的 edge-labels 行保留在类目中。

## 4. 独立评审（agent-run，记录在 ledger）

`tools/mermaid/requirement-f0-cohort.json` 的 review 块记录：独立 fresh-context 对抗评审 **r1 = REQUEST-CHANGES**（2 MAJOR：ledger 临时批准无收据、roster 未经工具迁移；4 MINOR：Work 欠计量、D-204 镜像陈述失真、尾随注释不一致、负例覆盖薄；5 NIT），全部修复后 **r2 仅余 1 MINOR**（D-204 生成器 `--check` 不可复现——补 `superseded_note` 后通过），评审结论 **APPROVE in substance**。Ledger 最终 verdict=approved 并记录 reviewer identity/日期/逐项处置。评审明确未替代 Main/B3 门（本里程碑不涉）。

## 5. xychart RFC 草案（交付 2）

`docs/rfcs/0035-xychart-envelope-and-config-admission.md`（commit `9a643e48`，**provisional**，未自批）：把 35 行 fallback 拆为 18 envelope 共存 + 9 config 值 + 8 能力行（逐行列出：带符号柱标签 2、零宽轴 1、逐点内联标签 4、>3 线系列 1），承诺最多 26 行、8 行明确 non-goal；含 D-394 复核修正（dataLabelColor 角色、x-axis-range-same-value 双计纠正、envelope 为 3 处具体代码改动）。待 agent design review + 业主确认 OQ-1（labelRotation 投影政策）。`rfc_lint.py` 对该文档零新增发现（既有 0033 红项为历史遗留）。

## 6. ER107 台账代际刷新（交付 3，commit `2b4443c2`）

常量分裂：程序身份 `BASE/FINAL = 688/795` 保留（compact worker verdict 实测仍发 688，不改）；新增矩阵代际 `MATRIX_FORMAL_BASELINE = 1053` / `MATRIX_FORMAL_JOINED = 1160` 仅用于 whole-matrix 覆盖与 `patch_matrix`。三 digest 以**未改动的 predicate** 对当前认证矩阵重算：`BASELINE_MATRIX_SHA = ee743515…`、`RETAINED_ROWS_SHA = 8b13df1f…`、`PRIOR_SELECTED_DEBTS_SHA = ed852811…`（旧值保留在审计注释）。三个 fail-closed 门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`、`WRITE_REVIEW_APPROVED`）未动；自检 15/15；`ctest -R Er107` 45/45。注：TL 简写 "688/795 → 1053/1239" 中 1239 是 observed drawn，而 formal 代际及其 join 后预期是 1053→1160；本实现按 formal 语义落值并在注释中说明，避免 formal/observed 混用。

## 7. NIT-1/NIT-2（D-393 交付，本轮复验）

早于本轮落地且仍在树内：NIT-1 `62580e0c`（16 个 literal 直接 seam 对等）、NIT-2 `1e61d60a`（计数由表派生）。本轮定向复验 `MermaidErSubgraphSourceProfile.*` 4/4 绿，无新增提交。

## 8. 验证矩阵（本里程碑）

- 定向：requirement cohort 全套 **10/10**（debug），release F0 用例 2/2；state E2b **6/6**；envelope **35/35**；ER107 `ctest -R Er107` **45/45**；group-edge/fallback 漂移门与自检均绿。
- 全量收口（单次，debug，`-j56`）：**4608 = 4607 通过 + 1 已知 flake + 25 运行期 SKIP + 2 编译期 DISABLED**（用例数较 D-390 的 4606 增加 2，即本轮新增的两个 F0 gtest）。唯一失败 `ServerMain.DirectConnectSessionsAndWebSocketUsePersistentRoutes` 属 AGENTS.md 载明的 TCP 时序 flake，隔离复跑 **Passed 0.34s**。
- 双预设：release 全树 `-Werror` 构建通过；矩阵字节一致（sha 同上）。

## 9. 未消解 / 推迟项

1. `edge-labels...` 行（无孪生）——待其自身 reviewed frame。
2. state/xychart/git 的 inert-only 行大多被 body 特性阻塞，不在 F0（D-394 复核结论）。
3. xychart RFC 待 design review 与 OQ-1/OQ-2 裁决；8 个能力行未排期。
4. ER107 B3 门继续不动，等 Main 确认；三级前置链与合约形状裁决仍未启动（见 D-392 记录）。
5. narrow_40 无承诺：8 行与 requirement 族其余 148 行一致保持窄屏回退。

## 10. 边界

十保护文件零改；`manifest.json` 及其 sha 级联零改；无 wire/持久化/settings/session 契约变更；无 theme/look 路径放宽；未 push、未 merge。

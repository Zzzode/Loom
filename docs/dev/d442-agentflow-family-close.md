# D-442 —— agentflow 封闭 `look: handDrawn` + `handDrawnSeed`（2 行，家族闭合）

日期 2026-11-23 · 本地，未 push · 承接 TL「下一批至少闭合一个家族的 fallback 计数」。

## 1. 选片理由与判据

- **为什么选它**：agentflow 是本轮唯一「只剩 2 行、且两行是同一个惰性组合」的家族。
  2 条残余的 frontmatter 都只有 `config.look: handDrawn` + `config.handDrawnSeed: 1`：
  rough.js 的手绘抖动与种子对终端转写没有任何语义，属惰性可披露面。
- **预期降幅**：2 行（agentflow 族 2 → **0**，全局 507 → 505，formal
  `drawn_at_240` 1452 → 1454）。
- **判据（语义）**：`config.look=handDrawn -> source node style; not applied` 与
  `config.handDrawnSeed=<n> -> sketch jitter seed; not applied` 追加在
  `Agentflow native projection v1` 脚注的既有 palette 行之后；画面（去脚注）与
  去掉 config 的孪生逐行相等。

## 2. 落地内容

- `src/ui/visual/mermaid_agentflow_impl.cpp`：RFC 0041 的「单一 theme 叶」包络
  扩展为封闭叶表 `{theme, look: handDrawn, handDrawnSeed: 正十进制(≤9 位、非零)}`，
  任意非空子集、逐叶查重；footer 保持 palette 行在前、look/seed 追加在后
  （既有 9 条已画帧字节不变）。
- 新门禁 `tests/ui/test_mermaid_agentflow_handdrawn_d442_cohort.cpp`（3 组：账本
  钉子／双孪生惰性／封闭负向 14 条）；生成器
  `tools/mermaid/generate_agentflow_handdrawn_d442_cohort.py`（`--check` 入 ctest，
  cohort digest `748c66a8…`）；矩阵分支带 rule-38 可达集证明。
- **负向迁移**（同提交完成）：`test_mermaid_agentflow_cohort.cpp` 与
  `test_mermaid_agentflow_0041_cohort.cpp` 中三处「这两行必须整源回退」的断言
  改为「由 D-442 门禁负责的已画行」，其余负例重指到仍被拒绝的形状
  （`fontFamily`、`look: neo`、seed `0`、`themeVariables` 映射、体部 init）。

## 3. 验证

- debug 定向：`ctest -R "MermaidAgentflow|MermaidStateLayoutD441"` **21/21 通过**；
  `MermaidCapabilityReport.OfficialMatrix` 通过。
- release 矩阵与批边界全量门见下一份批收口文档。

## 4. 账本收尾（2026-11-26 残留风险清理）

本片当时只迁移了 C++ 负例与矩阵分支，`agentflow-cohort.json` /
`agentflow-0041-cohort.json` 仍把这两行记为 `gated`，与矩阵"全族 0 回退"
矛盾。现两本账改为 `gated_count: 0` + `delegated_count: 2`（附
`delegated_ledger` 指向本片账本），生成器以本账本为唯一事实源交叉校验
schema、评审状态、sha 与 `wide_240: drawn`，两个家族 gtest 断言 delegated
集合与本账本行集相等。家族分区恒等式为 27 + 9 + 0 + 2 = 38。

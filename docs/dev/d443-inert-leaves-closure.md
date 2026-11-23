# D-443 —— requirement/ishikawa/venn 惰性 look/htmlLabels/layout 叶（6 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「本批连续推进到 fallback ≤ 480」。

## 1. 选片理由与判据

- **为什么选它**：对 roster 全部 505 行做了「去掉整个 config 块」的孪生探针
  （`draws(twin) && !draws(source)` 分类），可本地闭合的行只有 61 条：其中 42 条
  是 flowchart（Flow400 冻结面）、4 条 xychart 属历史债/真实渲染特性、9 条 ER 走
  `ER native projection v1` 的 ASCII 权威面（非本里程碑）。剩余 6 条正是
  requirement/ishikawa/venn 的封闭惰性叶：
  - requirement：`config.flowchart.htmlLabels`（3 行；2 行另带已准入的
    `look`/`layout`）；
  - ishikawa：`config.look: handDrawn`（1 行）；
  - venn：`config.look: handDrawn` + `handDrawnSeed` + `fontFamily: courier`（2 行）。
- **预期降幅**：6 行（全局 505 → **499**，formal `drawn_at_240` 1454 → **1460**）。
- **判据（语义）**：三族投影都自绘几何、从不读取这些叶；footer 追加在各自既有
  全部叶之后（已画帧字节不变），画面（去 footer）与 config-free 孪生逐格相等。

## 2. 落地内容

- `src/ui/visual/mermaid_requirement_impl.cpp`：`FlowNs/FlowHtml` 两位，允许
  indent-2 的空值 `flowchart:` 命名空间恰好一次、其 indent-4 的 `htmlLabels`
  叶取 `{false,true}`；命名空间无叶即整源回退；footer 末尾追加
  `config.flowchart.htmlLabels=<v> -> native text labels; not applied`。
- `src/ui/visual/mermaid_ishikawa_impl.cpp`：`look` 叶（闭合表 `{handDrawn}`），
  footer 追加在所有既有叶之后。
- `src/ui/visual/mermaid_venn_impl.cpp`：`look`（`{handDrawn}`）、
  `handDrawnSeed`（D-442 规则：1–9 位十进制、非全零）、`fontFamily`
  （`{courier}`）三叶，footer 追加在 theme 之后。
- 新门禁 `tests/ui/test_mermaid_d443_inert_leaves_cohort.cpp`（3 组：账本钉子／
  6 行双孪生惰性／三族封闭负向 20 条 + 组合正例）；生成器
  `tools/mermaid/generate_d443_inert_leaves_cohort.py`（`--check` 入 ctest，
  cohort digest `ead932f2…`）；矩阵分支带 rule-38 可达集证明。
- roster：**499 条 / 35 类**（`--emit` 迁移，删 6 增 0，`--self-test` 绿）；
  E6c 48 条未动。

## 3. 验证

- debug 定向：`MermaidD443InertLeavesCohort.*` **3/3 通过**（账本+生成器检查
  4 条）；`MermaidCapabilityReport.OfficialMatrix` 通过；
  矩阵 1646 drawn / 499 fallback / 48 gaps / integrity 0，formal 1460。
- 全量门与双预设字节门在本批边界统一执行。

## 4. 负向迁移

无既有断言被改写：这 6 行此前没有被任何旧门禁钉为「必须整源回退」，只登记在
roster，迁移即 `--emit` 重生成。

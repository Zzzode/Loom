# D-446 —— state 体部首部注释行准入（1 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」。

## 1. 选片理由与判据

- **为什么选它**：D-443/D-445 之后，state 只剩两行；其中
  `state-diagram-v2/nightly-check/elk/elk-notes-keep-their-layout.mmd` 的配置叶
  （`layout: elk`、`fontFamily: courier`）早已准入，唯一阻塞是**体部以三行
  `%%` 注释开头**：体部解析器本来就忽略注释（无 frontmatter 的裸体部可以画），
  只有 envelope 的 `raw_body.starts_with("stateDiagram")` 头部检查过严。
- **预期降幅**：1 行（全局 497 → **496**，formal 1462 → **1463**）。
- **判据（语义）**：只跳过**行首**的 `%%` 注释行（缩进注释、非注释行、注释后
  非 `stateDiagram` 头一律仍整源回退），跳过按 Work 计费；整帧（含 footer）与
  「删除这些注释行」的孪生逐格相等。

## 2. 落地内容

- `src/ui/visual/mermaid_relations_core_impl.cpp`：`state_source_envelope` 的
  头部检查改为先跳过行首 `%%` 行再比对 `stateDiagram`；其余状态分支不变。
- 新门禁 `tests/ui/test_mermaid_state_leading_comment_d446_cohort.cpp`（3 组：
  账本钉子／整帧孪生相等／仅行首注释被跳过 + 负向）；生成器
  `tools/mermaid/generate_state_leading_comment_d446_cohort.py`（`--check` 入
  ctest）；矩阵分支带 rule-38 可达集证明。
- roster：**496 条 / 35 类**（`--emit` 迁移，删 1 增 0，`--self-test` 绿）。

## 3. 验证

- debug 定向：`MermaidStateLeadingCommentD446Cohort.*` **3/3 通过**；
  `MermaidCapabilityReport.OfficialMatrix` 通过；矩阵 1649 drawn / 496
  fallback / 48 gaps / integrity 0，formal 1463。
- 全量门与双预设字节门在本批边界统一执行。

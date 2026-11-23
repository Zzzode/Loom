# D-447 —— usecase 封闭调优命名空间 + 无障碍声明（1 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」。

## 1. 选片理由与判据

- **为什么选它**：usecase 剩余 18 行里，16 行的体部还带 note/json/style/
  classDef/icon 等未支持的 body 特性；唯一只差「封闭配置 + 无障碍声明」的是
  `usecase/nightly-check/10-configuration-and-accessibility.mmd`。
  usecase 投影输出语句大纲、不画几何，`config.usecase.*` 的九个 SVG 调优叶
  对终态大纲是非终端的；`accTitle:`/块形 `accDescr { ... }` 是无障碍元数据，
  按 state 家族 RFC 0048 的既有先例作为「校验后 no-op」。
- **预期降幅**：1 行（全局 496 → **495**，formal 1463 → **1464**）。
- **判据（语义）**：九个调优叶逐值闭合、命名空间无叶即回退，逐叶披露；
  acc 声明校验（标量/块形、去重、空值拒绝）但不进入画面；去掉 config 与
  acc 声明的孪生与源的大纲部分逐格相等。

## 2. 落地内容

- `src/ui/visual/mermaid_usecase_impl.cpp`：新增 `usecase_frontmatter`（描述 +
  `config.usecase` 九叶闭合表 + 披露 footer）、`usecase_accessibility_value`；
  语句循环新增 `accTitle:`/`accDescr:` 标量与块形 no-op（重复/未闭合块/空值
  回退）；footer 追加在大纲之后。
- 新门禁 `tests/ui/test_mermaid_usecase_accessibility_d447_cohort.cpp`（3 组：
  账本钉子／叶与 acc 惰性孪生／封闭负向 15 条）；生成器
  `tools/mermaid/generate_usecase_accessibility_d447_cohort.py`（`--check` 入
  ctest）；矩阵分支带 rule-38 可达集证明。
- roster：**495 条 / 35 类**（`--emit` 迁移，删 1 增 0，`--self-test` 绿）。

## 3. 验证

- debug 定向：`MermaidUsecaseAccessibilityD447Cohort.*` **3/3 通过**；
  `MermaidCapabilityReport.OfficialMatrix` 通过；矩阵 1650 drawn / 495
  fallback / 48 gaps / integrity 0，formal 1464。
- 全量门与双预设字节门在本批边界统一执行。

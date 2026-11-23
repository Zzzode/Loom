# D-441 —— state 封闭 `config.layout: elk`（3 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续按普查推进下一族」。

## 1. 选片理由与判据

- **为什么选它**：D-440 之后，把 roster 全部 35 类逐条做了「去掉 config 的孪生」
  探针（510 行全量），剩 66 行孪生可画；按家族去重后，非冻结面的干净形状只剩
  state 的 `layout: elk`（3 行，另有 1 行体部含 note 仍回退）、ER 的
  `special_link_markers`（unicode 文本面）与 xychart 的标签旋转/点标签（渲染面）。
- **为什么是配置片**：state 包络（`state_source_envelope`）此前不认 `layout`，
  而终端本来就用自己的固定有向路由，ELK 引擎选择对终态投影是**非终端**语义——
  与 RFC 0044 对 requirement `layout: elk` 的准入完全同型。
- **预期降幅**：3 行（全局 510 → 507，formal `drawn_at_240` 1449 → 1452）。
- **判据（语义）**：`config.layout=elk -> external layout engine; not applied`
  披露在 `State native projection v1` 脚注末尾（追加在所有既有叶之后，已画帧
  字节不变）；画面（去脚注）与去掉 config 的孪生逐行相等。

## 2. 落地内容

- `src/ui/visual/mermaid_relations_core_impl.cpp`：state 包络新增
  `Layout = 128u` 位与缩进 2 的 `layout` 叶，值表只有语料支撑的 `elk`；标量走
  既有的共享 inert-scalar reader（单/双引号与行尾注释行为与其他叶一致）。
- 新门禁 `tests/ui/test_mermaid_state_layout_d441_cohort.cpp`（3 组：账本钉子／
  双孪生惰性／封闭负向 10 条 + 组合正例）；生成器
  `tools/mermaid/generate_state_layout_d441_cohort.py`（`--check` 入 ctest，
  cohort digest `033b701e…`）；矩阵分支带 rule-38 可达集证明。

## 3. 验证

- debug 定向：新门禁 **3/3**；`ctest -R "MermaidState|MermaidE2a|MermaidEr|MermaidRelations"`
  **218/218 通过**；`MermaidCapabilityReport.OfficialMatrix` 通过。
- release 矩阵与批边界全量门见 `d439-d441-batch-closure.md`。

## 4. 过程中的两次断言迁移（已记录）

首版把 `layout: 'elk'` 与 `layout: elk # note` 写成负例，实际被共享
inert-scalar reader 按「同一 YAML 字符串 / 去注释」接受；按迁移纪律把它们
移到正例（负例改用真正仍被拒绝的形状：`dagre`/`ELK`/空值/截断引号/重复/缩进 4/
未知兄弟叶）。

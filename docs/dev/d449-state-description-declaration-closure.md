# D-449 —— state `state X: Description` 声明形式（4 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」。

## 1. 选片理由与判据

- **为什么选它**：D-448 之后剩余的行里，`states-with-descriptions-neo-look-*`
  四行的配置叶（`look: neo`、`theme: X`）早已由 RFC 0038 准入，实测阻塞只有
  上游的声明拼写 `state State1: Description 1`——它给**已由转移行引入**的状态
  附加描述片段；解析器此前在 `entity.declared` 与 "stray token" 两处把它当作
  重复声明/残留文本拒绝。
- **预期降幅**：4 行（全局 494 → **490**，formal 1465 → **1469**）。
- **判据（语义）**：冒号形式在重复声明检查**之前**消费，不携带别名/花括号/
  标记，描述文本走既有的 `state_description_label` 校验并作为文本片段附加；
  画面（去 footer）与 config-free 孪生逐格相等。

## 2. 落地内容

- `src/ui/visual/mermaid_relations_core_impl.cpp`：`state ` 分支在
  alias/复合体之外新增 `: <description>` 形式——先校验并附加片段，再置
  `declared`，随后 `continue`。
- 新门禁 `tests/ui/test_mermaid_state_description_d449_cohort.cpp`（3 组：账本
  钉子／四行双孪生／声明形状正负例）；生成器
  `tools/mermaid/generate_state_description_d449_cohort.py`；矩阵分支带 rule-38
  可达集证明。
- roster：**490 条 / 35 类**（`--emit` 迁移，删 4 增 0，`--self-test` 绿）。

## 3. 验证

- debug 定向：`MermaidStateDescriptionD449Cohort.*` **3/3 通过**；
  `MermaidCapabilityReport.OfficialMatrix` 通过；矩阵 1655 drawn / 490
  fallback / 48 gaps / integrity 0，formal 1469。
- 全量门与双预设字节门在本批边界统一执行。

## 4. 过程中否掉的邻近路线（实测）

- `basic-states` 全连通 5 状态：实测 8 条转移可画、第 9 条即回退——是 relations
  布局的容量面（共享几何契约），不是语法缺口，超出本里程碑。
- `should-render-a-simple-state-diagrams-with-labels` 等：其体部含 5 路扇出 +
  `--> [*]` 的组合，仍在布局容量内失败，未纳入。

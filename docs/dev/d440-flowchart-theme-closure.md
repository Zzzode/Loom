# D-440 —— flowchart 封闭 `config.theme`：1 行准入 + Flow400 冻结面实测

日期 2026-11-23 · 本地，未 push · 承接 TL「继续按普查推进下一族」。

## 1. 选片理由与判据

- **为什么选 flowchart**：普查显示 flowchart 侧最大的「形状封闭」残块是
  frontmatter `look`/`theme`（40 + 12 + 16 … 行）。先按 D-438 教训用一次性探针
  测「去掉 config 的孪生」：全 corpus 里 frontmatter 仅含 `description + config`
  且 `config` 只含缩进 2 的 `look`/`theme`/`flowchart` 叶、孪生可画的共 17 行，
  其中 flowchart 头 11 行（3 look + 8 theme）、其余 6 行属于其他家族。
- **实测转折（本片核心发现）**：把 `theme` 的封闭表取
  `{redux, redux-dark, redux-color, redux-dark-color, forest}` 落地后，debug 矩阵
  直接报 **`flows.errors = 8`**、`mandatory gaps 48 → 57`，8 条
  `flowchart/nightly-check/redux-color-subgraphs/*` 全部命中
  `flow_whole_null_unmet`。原因：**`flowchart/` 下的 400 条 fixture 恰好等于冻结的
  Flow400 合同全集**（400 源、8 个 complete model、392 个 pending/null），
  pending 行不许被渲染器画出——否则就是合同外的「unexpected renderer-only」。
  即：flowchart 家族的整源回退**整体**被 Flow400 合同冻结，任何 flowchart 配置片
  都必须先做 Flow400 重基线（建 model + 重算 SHA），属受保护面，需 TL 裁决。
- **本片收缩后的准入**：封闭表取 **`{forest}`**——唯一在 Flow400 合同之外、
  且孪生可画的一行：`conf-and-directives/pr-check/settings-from-initialize-nodes-should-be-green.mmd`
  （头为 `graph TD`，不是 `flowchart/` 路径，不在 400 选集内）。
- **预期降幅**：1 行（全局 511 → 510，formal `drawn_at_240` 1448 → 1449）。
- **判据（语义）**：终端不画调色板，`theme` 惰性并披露
  （`config.theme=forest -> colour palette; not applied`）；画面（去脚注）与去掉
  config 的孪生逐行相等。

## 2. 落地内容

- `src/ui/visual/mermaid_impl.cpp`：`flow_envelope_body` 新增
  `Theme = 16384u` 与「孤立 `theme` 叶」精确位型（`fields == Theme`）；值取
  `forest`（裸写或双引号均可），其余 palette、单引号、失败引号、注释、重复叶、
  同名兄弟叶（`look`/`htmlLabels`/`flowchart` 命名空间）一律整源回退。
  `FlowEnvelope.theme` 在 flow 布局里追加披露行（沿用 FS-1 的通知通道）。
- 新门禁 `tests/ui/test_mermaid_flow_theme_d440_cohort.cpp`（3 组：账本钉子／
  双孪生惰性／封闭负向 17 条）；生成器
  `tools/mermaid/generate_flowchart_theme_d440_cohort.py`（`--check` 入 ctest，
  cohort digest `fee3e64b…`）；矩阵分支带 rule-38 可达集证明。

## 3. 验证

- debug 定向：新门禁 **3/3**；`MermaidCapabilityReport.OfficialMatrix` 由
  「broad 表失败（errors=8）」修正后通过。
- release 全量重编 + 矩阵见下方收口段（按 TL 口径，全量 ctest 留到批边界）。

## 4. 记录给 TL 的阻塞面（本轮新增）

- **Flow400 合同冻结 flowchart 全族 400 条**：其中 392 条是 pending/null，
  没有 model 就不能合法画出。这不是配置片能解决的，需要 Flow400 重基线
  （与 ER107 join、Block58 join 同型的受保护工作）。本片只吃掉合同外的 1 行。

## 8. 契约扩展记录（TL 第 804/809 轮授权）

**这是有意的契约扩展，不是漂移。** 第 804 轮授权按组重基线
`redux-color-subgraphs` 八行；第 806 轮授权画布派生的有界 Flow Work 信封与
`disclosure_footer` 已评审路径；第 809 轮裁定「按类准入、分类不补 Model」，
并以实测推翻 806 轮对 4 个 collapsed 兄弟行的「语法未建模」归因（FS-7b 已支持
collapsed 分组，基线里它们的 body 在 240/40 宽度下都已画出）。

落地形态：`flow_envelope_body` 的 palette 表扩为
`{forest, redux, redux-color, redux-dark, redux-dark-color}`；十八行准入走
`tools/mermaid/d470-projection-draws-cohort.json` 的已评审投影绘制 cohort
（每行一句具体因果 + 源字节钉 + 披露脚注钉），Flow400 oracle 未改动、不认脚注。
详见 `docs/dev/d470-flow-envelope-and-admission-scope.md`。

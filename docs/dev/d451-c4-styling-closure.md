# D-451 —— c4 闭合样式调用 + 可访问性 no-op 准入（6 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」（485 → ≤480）。

## 1. 选片理由与判据

- **为什么选它**：D-450 之后剩 5 行缺口；c4 残差 6 行实测后唯一阻塞是
  `UpdateElementStyle(…)` / `UpdateRelStyle(…)` 调用与标量
  `accTitle:`/`accDescr:`——三者都落在**已有先例**内（D-445 的惰性调用披露、
  RFC 0048/D-447 的 acc no-op），不需要共享渲染契约。
- **根因（实测）**：c4 视觉门显式把两个 `Update*Style` 关键字判为回退；语句
  循环对无括号的 `accTitle:` 行返回未知语句。
- **判据（语义）**：
  - `UpdateElementStyle(alias, $attr...)` 至少一个属性、`UpdateRelStyle(from,
    to, $attr...)` 至少一个属性；属性是**闭合形状** `$ident=value`（键不枚举，
    与元素 `$attr` 表同一约定），值为非空且不含括号；重复属性整源回退；
    无属性、空值、缺 `$`、块体、未知调用 → 回退；
  - 每条属性一行披露：`UpdateElementStyle(a).$fontColor -> SVG styling;
    not applied`（源顺序；行 ≤ 96 字节）；
  - `accTitle:`/`accDescr:` 标量、非空、≤256 字节、unicode、去重；块体形式
    仍回退（语料未出现）。
- **为什么安全**：c4 投影是大纲文本，不画颜色/几何；c4-1 的嵌套
  Enterprise_Boundary 已在既有词表中。

## 2. 落地内容

- `src/ui/visual/mermaid_c4_impl.cpp`：删除两个关键字的预扫描门；语句循环
  新增 acc 分支与 `Update*Style` 分支；页脚在 D-445 行之后按源顺序追加；
  新增 view-only `trim_view` 助手。
- 新门禁 `tests/ui/test_mermaid_c4_styling_d451_cohort.cpp`（4 组：账本钉子／
  样式对无样式孪生惰性／闭合负向／生成器可复现）；生成器
  `tools/mermaid/generate_c4_styling_d451_cohort.py` + 账本
  `tools/mermaid/c4-styling-d451-cohort.json`（digest `abc1c6ba…`）。
- 矩阵：exact-path 分支置于 D-445/D-432/RFC 0031 c4 分支之前，带 rule-38
  可达集证明；`MermaidC4Cohort` 的 `styled_excluded` 9 → 0（人口整体转入
  D-432/D-445/D-451 三本账本），`MermaidC4LayoutConfigD445Cohort` 的
  “其他 Update* 回退”负例改点为**无属性/缺 `$`** 形状。
- roster `--emit` 迁移：479 条 / 33 类。

## 3. 验证

- debug 定向：`MermaidC4StylingD451Cohort.{LedgerSchemaReviewsAndPins,
  StylingIsInertAgainstStylingFreeTwins, ClosedStatementNegatives,
  GeneratorIsReproducible}` 全绿；`MermaidC4*` 四组门禁全绿。
- 矩阵：1666 drawn / **479 fallback** / 0 error / formal 1480 / gaps 48 /
  integrity 0 ——**达标（≤480）**。
- 全量门与双预设字节门在批边界统一执行。

## 4. 遗留

- `ER native projection v1` ASCII 权威面、Flow400 冻结面、swimlanes 无渲染
  器等合同级面不变；横向 xychart 一行仍待 TL 授权的 E6c 花名册对账。

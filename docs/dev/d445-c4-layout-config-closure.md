# D-445 —— c4 封闭 `UpdateLayoutConfig` 调用（2 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」。

## 1. 选片理由与判据

- **为什么选它**：D-443 之后的剩余面里，c4 的两行
  `update-layout-config-{2,4}-shapes-per-row` 是唯一「阻塞物是一句闭合调用且
  没有几何语义」的形状：原生 c4 投影输出的是**语句大纲**（每个元素一行
  `System a - A`），不画几何图，因此上游 SVGC4 的每行形状数
  （`$c4ShapeInRow`）与边界行数（`$c4BoundaryInRow`）对终态大纲是惰性的。
- **预期降幅**：2 行（全局 499 → **497**，formal 1460 → **1462**）。
- **判据（语义）**：调用被校验且**不产生任何大纲行**，只在
  `C4 native projection v1` 脚注（D-432 的 wrap 行之后）追加一行披露；
  画面（去 footer）与「删掉该调用行」的孪生逐格相等。

## 2. 落地内容

- `src/ui/visual/mermaid_c4_impl.cpp`：
  - 预扫描不再把 `UpdateLayoutConfig` 当作硬门禁（`UpdateElementStyle` /
    `UpdateRelStyle` 仍然硬门禁）；
  - 语句循环新增分支：恰好一次、恰好两个引号参数、顺序固定
    `$c4ShapeInRow="2"|"4"`、`$c4BoundaryInRow="1"`，逐个闭合表校验，
    重复/多余/未加引号/带块/顺序颠倒一律整源回退；
  - footer 改为披露列表：`config.wrap` 行保持第一，D-445 行追加其后。
- 新门禁 `tests/ui/test_mermaid_c4_layout_config_d445_cohort.cpp`（3 组：账本
  钉子／调用无关双孪生／封闭负向 11 条 + 正例）；生成器
  `tools/mermaid/generate_c4_layout_config_d445_cohort.py`（`--check` 入
  ctest，cohort digest `6d1a92ee…`）；矩阵分支带 rule-38 可达集证明。
- roster：**497 条 / 35 类**（`--emit` 迁移，删 2 增 0，`--self-test` 绿）；
  E6c 48 条未动。

## 3. 验证

- debug 定向：`MermaidC4LayoutConfigD445Cohort.*` **3/3 通过**；
  `MermaidC4*` 既有门禁全绿；`MermaidCapabilityReport.OfficialMatrix` 通过；
  矩阵 1648 drawn / 497 fallback / 48 gaps / integrity 0，formal 1462。
- 全量门与双预设字节门在本批边界统一执行。

## 4. 断言迁移

D-432 门禁里 `UpdateLayoutConfig($c4ShapeInRow="2")`（**单参数**）负例继续有效
（新分支要求恰好两个参数），无需改写；新门禁补齐双参数表外值与其他 Update*
语句的负例。

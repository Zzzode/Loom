# D-450 —— quadrant 首行 `%%{init}%%` 闭合准入（5 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续下一片」（490 → 480）。

## 1. 选片理由与判据

- **为什么选它**：D-443..D-449 之后剩余 10 行，逐一实测后唯一无需动共享
  渲染契约的族内面是 quadrant-chart 的 5 行——它们的唯一阻塞是**体部首行**
  `%%{init: {...}}%%` 指令。
- **根因（实测）**：quadrant 渲染器的视觉门把任何 `%%{init` 行判为回退，且
  语句循环把 `%%` 行当注释跳过，因此指令无法被消费。投影本身是纯文本大纲
  （title/轴/Q1-4/点坐标），**不画轴、不画点、不用颜色**，所以轴位置、尺寸、
  字号、内边距与主题色全部是惰性信息。
- **判据（语义）**：只有**首行**指令可被消费；指令内为闭合表：
  - `quadrantChart` 子对象：16 个尺寸/内边距键（非负整数）+ `xAxisPosition`
    {top,bottom} + `yAxisPosition` {left,right}；
  - `themeVariables` 子对象：15 个 quadrant 颜色键，值必须是 `#rrggbb`；
  - 裸 `%%{init}%%` 与空对象 `{}` 是合法 no-op；
  - 未知键/未知节/重复键/重复节/嵌套值/非十六进制颜色/第二条第指令/
    非首行指令 → 整源回退。
- **披露**：`Quadrant native projection v1` + 每个键一行（源顺序），
  `%%{init}%% quadrantChart.<k>=<v> -> SVG layout/axis tuning; not applied`
  或 `-> colour palette; not applied`。

## 2. 落地内容

- `src/ui/visual/mermaid_quadrant_impl.cpp`：新增闭合指令解析器（顶层逗号
  切分跟踪引号与花括号、键去引号、标量校验、颜色校验），首行消费后进入原
  视觉门；页脚在 art 之后追加。
- 新门禁 `tests/ui/test_mermaid_quadrant_init_d450_cohort.cpp`（4 组：账本
  钉子／指令对无指令孪生惰性／闭合负向／生成器可复现）；生成器
  `tools/mermaid/generate_quadrant_init_d450_cohort.py` + 账本
  `tools/mermaid/quadrant-init-d450-cohort.json`（digest `997dd7d0…`）。
- 矩阵：exact-path 分支置于 RFC 0031 quadrant 分支之前，带 rule-38 可达集
  证明；`MermaidQuadrantCohort` 的 `init_gated` 5 → 0（人口整体转入本账本）；
  roster `--emit` 迁移。

## 3. 验证

- debug 定向：`MermaidQuadrantInitD450Cohort.{LedgerSchemaReviewsAndPins,
  InitIsInertAgainstInitFreeTwins, ClosedDirectiveNegatives,
  GeneratorIsReproducible}` 全绿；`MermaidQuadrantCohort.*` 迁移后全绿。
- 矩阵：1660 drawn / 485 fallback / 0 error / formal 1474 / gaps 48 /
  integrity 0。
- 全量门与双预设字节门在批边界统一执行。

## 4. 遗留

- xychart 旋转三债（约定不动）、横向负值标签 profile（受保护 E6c 观测钉住）
  维持原状。

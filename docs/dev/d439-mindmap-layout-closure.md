# D-439 —— mindmap 封闭 `config.layout: tidy-tree`（3 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「继续按普查推进下一族」。

## 1. 选片理由与判据

- **为什么选它**：`family_residual:mindmap`（10 条）是当前最大的「形状封闭」残余族。
  其中 5 条只差 `config: layout: tidy-tree`；其余是图标（5 条）与 Markdown 格式化
  （1 条），属外来视觉面。
- **先探体部（D-438 教训）**：用一次性探针逐条测「去掉 config 的孪生」，结果 5 条里
  只有 3 条体部在既有文法内；另 2 条的真实阻塞是体部——1 条带
  `::icon(fa fa-book)`（图标面未准入），1 条以匿名 `((This is a mindmap))`
  顶层根简写开头（文法不解析）。账本因此按**实测 3 行**定尺，不按配置检测器的桶。
- **预期降幅**：3 行（mindmap 族 10 → 7，全局 514 → 511，formal `drawn_at_240`
  1445 → 1448）。
- **判据（语义）**：tidy-tree 是外部布局引擎，终端不运行；叶子惰性、必须披露
  （`Mindmap native projection v1` + `config.layout=tidy-tree -> external layout engine;
  not applied`）；判据是「画面（去脚注）与去掉 config 的孪生逐行相等」。

## 2. 落地内容

- `src/ui/visual/mermaid_mindmap_impl.cpp`：`mindmap_body` 从「只认 description」
  扩展为「description + 可选 `config:` → 唯一缩进 2 的 `layout: tidy-tree`」，
  并以 `parse_envelope` 的 config 事件做交叉校验（单叶、indent 2、纯标量、
  非命名空间条目）。其余一律整源回退：其他 layout 值、其他叶子、init、title、
  注释、重复叶、错位缩进、`config` 先于 `description`。
- 披露脚注只在源带该 config 时追加；config-free 帧逐字节不变（保住 Original10 /
  Shapes4 的既有帧）。
- 新门禁 `tests/ui/test_mermaid_mindmap_tidy_d439_cohort.cpp`（3 组：账本钉子／
  双孪生惰性／封闭负向 14 条 + 体部负向 3 条）；生成器
  `tools/mermaid/generate_mindmap_tidy_d439_cohort.py`（`--check` 入 ctest，
  cohort digest `36d3a7cf…`）；矩阵分支带 rule-38 可达集证明。

## 3. 验证

- debug 定向：`ctest --preset debug -R "Mindmap|Shapes4|MermaidE2a"` → **24/24 通过**；
  新门禁 3/3。
- release 矩阵（全量重编 209/209 退出码 0）：
  original **drawn 1634 / fallback 511 / error 0**，`drawn_at_240` **1448**，
  gaps 48，integrity 空；`MermaidCapabilityReport.OfficialMatrix` Passed。
- roster 重生成：**511 条 / 35 类**，`family_residual:mindmap` 10 → **7**，
  `--self-test` 绿。
- 全量 ctest 与双预设字节门按 TL 口径留到本批边界。

## 4. 过程中的一次返工（已记录）

首版把 D-439 的 cohort map 插在 `JourneyCohortEntry` 声明之前，release `-Werror`
直接报未声明标识符；同时旧二进制「矩阵通过」是陈旧链接的假绿。修复=移动声明位置，
并按规则重编全量后再信矩阵结论。

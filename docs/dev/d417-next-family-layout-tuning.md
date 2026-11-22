# D-417 —— 下一片选择：layout/display 调参命名空间（ishikawa + architecture + state，11 行）

日期 2026-11-22 · 本地，未 push · 承接 TL 常驻授权（RFC 0045 验收通过后继续下一片）。
排除项不变：受保护 43 条、flowchart exact-bit 222 条、三笔历史债（xychart rotation
3 行、narrow_40、ER107 B3）。

## 1. 判据

与 D-413/D-415 相同：①文本惰性可证（渲染器不读该键，且去 frontmatter 后 body 在
240 列原生绘制）；②机制成本（复用/最小扩展既有 frontmatter 门）；③行数 / 单位成本；
④排除改变绘制内容的键。

## 2. 家族实测（628 条 roster，2026-11-22 收口后）

| 候选 | 行数 | 键形状 | 去 config 可绘 | twin | 判定 |
|---|---|---|---|---|---|
| **ishikawa `ishikawa.useMaxWidth` + `diagramPadding`** | 3 | `config: ishikawa: useMaxWidth {true,false}` / `diagramPadding {50}` | 是（D-410 桶 5a） | 混合（diagramPadding 1 corpus + 其余 synthetic） | **选中** |
| **architecture `architecture.randomize/seed/idealEdgeLengthMultiplier`** | 4 | 同一命名空间，三叶 | 是（5a） | 混合（idealEdgeLengthMultiplier 1 corpus + 其余 synthetic） | **选中** |
| **state `state.useMaxWidth`（v1 2 + v2 2）** | 4 | `config: state: useMaxWidth {true,false}` | 是（5a） | 合成 | **选中** |
| pie `donutHole`/`highlightSlice`/`textPosition` | 8 | 几何/内容 | 是（5a） | — | 排除：改变绘制内容（饼图 vs 环形、切片高亮） |
| treemap `treemap.valueFormat` | 6 | 数值格式 | 是（5a） | — | 排除：改变标签文本 |
| cynefin `boundaryAmplitude/seed/width/height/showDomainDescriptions` | 5 | 几何/内容 | 是（5a） | — | 排除：边界几何与域描述可见性 |
| state/state-v2 标准 `theme`（dark/default/forest/neutral） | 8 | body 内 `%%{init}%%` | 否 | — | 排除：需 body-init 机制（跨家族设计），非本片 |
| mindmap `look`+`theme` | 6 | 闭合表 | **否**（5c body 阻塞） | — | 排除：all-shapes body 不绘 |
| sankey `config.sankey.*` | 17 | 布局/颜色 | **否**（5c：plain-sankey 方言 body 不支持） | — | 排除：不是 config 债 |
| gitGraph 内容键（showBranches 等） | 32 | 内容语义 | 部分 | — | 排除：改变绘制内容 |
| flowchart 其余 fallback | 180 | 多类 | 部分 | — | 排除：exact-bit 认证 profile 保留面（222 条口径） |

## 3. 选中项结构（实测）

- **ishikawa 家族**：13 originals，RFC 0043 后 **9 drawn / 4 fallback**；其中 3 条是
  `config.ishikawa.*`（useMaxWidth ×2、diagramPadding ×1），1 条是 `look: handDrawn`
  （RFC 0043 明确不回退例外，保持）。既有 `ishikawa_frontmatter_gated` 行扫描已支持
  `config.theme`，本片把它扩成「一个 `ishikawa` 命名空间 + 两个闭合值叶」。
- **architecture 家族**：20 originals，**16 drawn / 4 fallback**；4 条全部是
  `config.architecture.*`（randomize {true,false}、seed {42}、
  idealEdgeLengthMultiplier {3}）。现有门是「见 config 即回退」的平铺扫描，本片改为
  与该扫描同风格的结构化 config 块（单命名空间、三叶、闭合值），并新增页脚通道。
- **state 家族（v1 + v2）**：实测 v1 **28 drawn / 14 fallback**、v2 **52 / 51**；其中的
  4 条是 `config.state.useMaxWidth`（v1 2、v2 2）。既有 `state_source_envelope` 是
  `parse_envelope` 门，本片按 RFC 0045 的命名空间形状 admit（indent-2 `state` +
  indent-4 `useMaxWidth`），页脚追加在既有行之后。
- twin 选取（设计评审 r1 更正）：**2 条有干净 corpus twin** —— ishikawa
  `pr-check/9-…diagrampadding.mmd` 的 body 与已绘制的 `pr-check/1-…simple-ishikawa…`
  逐字节相同（RFC 0043 已把 fixture 1 用作 theme 行的 corpus twin），architecture
  `…increased-idealedgelengthmultiplier.mmd` 与已绘制的 `…default-fcose-knobs.mmd`
  逐字节相同 → 这两条用 `kind: "corpus"`。其余 9 条用 synthetic config-free twin
  （删除 `config:` 行及其后所有 indent ≥ 2 行，直到下一个列 0 键）；其中 state 4 条
  确有 body 等价 fixture，但那些 twin 自带 E2b `logLevel`/`fontFamily` 页脚，不构成
  干净 corpus twin。
- 页脚统一文案：`config.<path>=<v> -> <display sizing|diagram padding|layout tuning>;
  not applied`，行宽均 < 96 字节。

## 4. 目标与副作用（预期）

- 全局 1517/628 → **1528/617**；formal `drawn_at_240` 1331 → **1342**；gaps 48、
  integrity 0 不变；ishikawa 9/4 → 12/1；architecture 16/4 → 20/0；
  state v1/v2 各 +2。
- roster 628 → **617**；`family_residual:ishikawa` 2 → 0、`family_residual:architecture`
  4 → 0（类别可能 43 → 41，收口实测）；`dagre_layout_tuning` 18 → 17。
- 三数报告预测：①**617** ②**289**（未变）③**11**。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

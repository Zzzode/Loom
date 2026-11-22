# D-413 —— 下一家族选择：能力债 374 中的 requirement `layout: elk`（23 行）

日期 2026-11-22 · 本地，未 push · 承接 TL 指令（RFC 0043 PASS 后立即选片，
不等待口径决定；避开受保护 43 与 flowchart exact-bit）。

## 1. 判据（与 D-408/D-411 相同，按实测）

从 D-410 桶 5 的 374 条（5a 语义键 106 + 5b 形状 15 + 5c body 253）中选片，排序依据：

1. **文本惰性可证**：渲染器从不读取该键，且存在 body 逐字节相同的已绘制 twin
   （corpus）或可合成 config-free twin；框架身份 = twin 帧 + 披露页脚。
2. **机制成本**：优先复用既有准入通道（已有 notice/页脚的 TU）而不是新造闸门。
3. **行数 / 单位成本**：一个机制面覆盖的行数。
4. **排除**：受保护 gap roster（43）、flowchart exact-bit（222）、会改变绘制内容的键
   （valueFormat、seed、showBranches 等）。

## 2. 候选实测（2026-11-22，结构化解析 + 当前 663 条 roster）

| 候选 | 行数 | 机制面 | twin | 判定 |
|---|---|---|---|---|
| **requirement `config.layout: elk`** | **23**（`elk_layout_engine` 24 减去 1 条无 twin 的 flowchart 嵌套行） | 既有 `requirement_envelope` 准入通道再增一个闭合表叶子 + 一条既有页脚行 | **23/23 corpus twin**（与非 elk 已绘制行 body 逐字节相同） | **选中** |
| git `gitGraph.parallelCommits` | 12 | git 平铺闸门新增一个布尔叶子 | 待测 | 次选：RFC 0039 已把 namespaced `config.gitGraph.*` 整体判为语义债，单键切分需额外论证 |
| pie `donutHole` + `highlightSlice` | 5 | charts TU 新 profile | 部分 | 次选：两个叶子、语义面更宽 |
| treemap `valueFormat` | 6 | — | — | 拒绝：改变显示的数字格式（非惰性） |
| architecture `randomize`/`seed`/`idealEdgeLengthMultiplier` | 4 | — | — | 拒绝：seed 形状（RFC 0039/0040 明确排除） |
| xychart `labelRotation`/`showLabel` | 2 | — | — | 拒绝：rotation 属三笔历史债 |
| tree-view 图标/行距 | 4 | — | — | 拒绝：图标字形属 foreign-engine |
| mindmap `layout` / state `useMaxWidth` / c4 `wrap` | 3+4+1 | 各自 TU | 待测 | 暂缓：行数少、机制面分散 |

## 3. 选中项的结构（实测）

- requirement 家族：originals 172，当前 **144 drawn / 28 fallback**。
- 28 条 fallback 中，`config.layout: elk` 占 **24**；全部落在 roster 类别
  `elk_layout_engine`（该类共 38，家族外另有 14）。
- 24 条中：15 条只有 `layout`，8 条 `layout + htmlLabels: false`（`htmlLabels` 已由
  D-066 F0 准入），1 条为 `config.flowchart.htmlLabels` 嵌套映射（无 corpus twin，
  且 `requirement_envelope` 现规则拒绝 namespaced entry）→ **本片不做，保留回退**。
- 本片之后 requirement 剩 5 条 fallback：2 条 `config.flowchart.htmlLabels` 嵌套映射
  （含 1 条同时带 `look`；`requirement_envelope` 拒绝 namespaced entry）、1 条仅 `title:`
  （envelope 现要求 description）、1 条无 frontmatter 的 body 能力债、1 条无 corpus twin
  的 elk 行（本片明确排除）。
- `layout: elk` 对 requirement 文本投影是惰性的：原生投影只输出 requirements/
  elements/connections 的文本树，不绘制 ELK 几何；23 条 body 与已绘制非 elk 行逐字节
  相同。披露页脚声明 `external layout engine; not applied`。
- narrow_40 需现场实测（页脚行 > 40 列，预期 0/23）。

## 4. 目标与副作用（预期）

- requirement 144/28 → **167/5**；全局 1482/663 → **1505/640**；
  formal `drawn_at_240` 1296 → **1319**；gaps 48、integrity 0 不变。
- roster 663 → **640**；类别 `elk_layout_engine` 38 → **15**（类别不消失，43 类不变）。
- 三数报告预测：①**640** ②**289**（未变）③**23**。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

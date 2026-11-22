# D-421 —— 下一片选择：state 复合/并发体几何（27 行）

日期 2026-11-22 · 本地，未 push · 承接 RFC 0047 验收（TL：直接开工下一片，顺手治 #2621 预算）。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、**ER107 冻结面**。

## 1. 方法（与 D-419 同法，rule 39）

对 roster 全部 **608** 条 fallback 做两种剥离后的公共 seam 实测（临时探针
`Probe0048`，已删除；逐行结果 `/tmp/loom-logs/probe-0048-debt.jsonl`，
汇总 `/tmp/loom-logs/probe-0048-summary.txt`）：

- **A**：删整个 frontmatter + 所有前导 `%%{…}%%` 行；
- **B**：只删 `config:` 块（含 indent≥2 子行）+ 前导指令，其余 frontmatter 保留。

结果：**A 与 B 完全相同**——161 行可绘、447 行仍不可绘。即不存在“被非 config
frontmatter 键单独挡住”的行；config-only 池 = 161 行。

## 2. 结论一：惰性 config 井已接近见底

161 行按 config 叶分类后，**叶集全部属于惰性类**（look/theme/htmlLabels/
useMaxWidth/logLevel/themeVariables）的只有 **9 行**，且集中为：

| 家族 | 行数 | 叶 | 判定 |
|---|---|---|---|
| er-diagram | 7 | `look: handDrawn`（3）、`htmlLabels: false`（2）、两者（2） | **不可动**：这些行属 ER107 whole-null 冻结面（矩阵 reason 为 “Whole-null ER original retains complete source/config/foreign debts”） |
| ishikawa | 1 | `look: handDrawn` | 可准入，但仅 1 行 |
| conf-and-directives | 1 | `theme` | 非家族根，单行 |

其余 152 行携带内容/几何键（`showBranches`/`showCommitLabel`/`rotateCommitLabel`、
`pie.donutHole`/`highlightSlice`/`legendPosition`/`textPosition`、`treemap.valueFormat`、
`quadrantChart.xAxisPosition`、`cynefin.boundaryAmplitude`/`seed`/`width`/`height`、
`radar.axisScaleFactor`、`packet.showBits`、`tree-view` icon 映射、
`timeline.disableMulticolor`、`gitGraph.*` 内容键、`layout: elk`/`flowchart.htmlLabels`
在非 ER 家族的组合等）。终端无法在不改变画面的前提下兑现，按既有政策保持回退。

**因此不再有“闭值惰性配置”级别的下一片**；ER 尾部被 ER107 冻结，ishikawa 单行
不值得独立一片。

## 3. 结论二：最大的连贯 body 能力池 = state 复合/并发几何

非冻结、非排除（flowchart 222、swimlanes 46、ER107 138）的最大单一 body 池是
**复合/并发状态几何**：state v1 7 + v2 20 = **27 行**，全部在剥离 config 后仍
不可绘（body 债），特性分布实测：

| 阶段 | 形状 | 行数 | 代表 |
|---|---|---|---|
| P1 | 并发区域（`--` 分隔列） | 11 | `concurrency-composite-containers-*`（4）、`regions-inside-nesting-*`（4）、`should-align-dividers-correctly`（1）、v1/v2 `should-render-concurrency-states`（2） |
| P2 | 前导 `%%{init}%%` + `accTitle`/`accDescr`（body 同样含区域，依赖 P1） | 8 | v1+v2 `{dark,default,forest,neutral}-theme` |
| P3 | 带标签边的复合/fan-out 布线 | 7 | 3 个 next-line-brace 复合、v1/v2 `…simple-state-diagrams-with-labels`、v1/v2 pr `composite-states` |
| 排除 | 非 state 体（`info` 指令） | 1 | `v2-should-render-a-simple-info` |

> 修订 2026-11-22（同日，RFC 起草时）：初版把“复合容器（titled box）”当作待建能力，
> 经渲染管线勘察确认 **复合容器几何已存在并已绘制**（`EntityShape::Group` 递归布局，
> state-0038 账本已钉 28 行，其中 14 行含 `state X {`），27 条残余的真实阻断是上表四类。
>
> 修订 2（RFC 0048 r1 评审后）：P3 的 7 行**从 RFC 0048 移出**——其阻断标签
> （`state-composite-brace-on-next-line`、`state-native-routing-debt`）未建立布局根因，
> 且首轮提出的 retry 机制未被证成并会翻倍布线 Work。本片实际范围 = **P1 11 + P2 8 = 19 行**：
> state v1 **12 → 7**、v2 **49 → 35**，全局 **608 → 589**，formal **1351 → 1370**；
> 三数 ①**589** ②**289** ③**19**。P3 与 `info` 行留给后续 RFC。

目标（P1–P3 全部落地）：state v2 **49 → 29**、state v1 **12 → 5**，全局
608 → **582**；formal 1351 → **1377**。三数预测：①**582** ②**289** ③**26**
（`info` 行移出本 RFC 范围）。

## 4. #2621 预算（已完成，独立提交 `fef4aff0`）

`MermaidOfficialConfig110Native…` 300 s → 600 s、`MermaidBlock58AdmissionJoin…`
60 s → 180 s，按实测（213/250/260 s；25 s）标定；断言不变。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

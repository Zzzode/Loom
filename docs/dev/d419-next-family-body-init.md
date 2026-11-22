# D-419 —— 下一片选择：journey 家族 in-body `%%{init}%%` 惰性指令（9 行）

日期 2026-11-22 · 本地，未 push · 承接 TL 常驻授权（RFC 0046 验收通过后继续）。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债。

> 修订 2026-11-22（同日，实施前）：初版按“行内含 `%%{`”的粗略正则统计为 17 条并
> 把 timeline 记为 6 条；逐行结构化复测后修正为：fallback 中带**前导正文指令**的
> 行共 67 条（含 frontmatter `description` 折叠块里提到指令的散文假阳性）；其中
> journey 家族可绘且可准入的是 journey 4 + timeline 5 = **9 行**。timeline 的
> 另外 2 条（`disableMulticolor` 内容键、多行 `themeVariables` 覆写）不可准入。
> 初版的 “timeline 6” 把 `disableMulticolor` 行也计入了可绘集合，但它不属于惰性
> 指令。以本修订为准。

## 1. 判据与实测

RFC 0046 后 roster 617 条 / 41 类。对全部 fallback 行复核：

1. **body 行（frontmatter 之后）以 `%%{` 开头的行共 67 条**，按家族：flowchart 34、
   git 9、timeline 7、journey 4、quadrant 5、state v1 4、v2 4。
2. **去 frontmatter 且去所有前导 `%%{init…}%%` 行后仍可绘**（经公共
   `render_markdown` seam，240 列；本轮专项探针 `ProbeInitTmp`，日志
   `/tmp/loom-logs/probe-0047-selection.log`，探针源已删除）：**9/9 可绘**
   （journey 4、timeline 5）。state 8 条不可绘（body 为并发区/复合态能力债）。

| 候选 | 行数 | 机制面 | 判定 |
|---|---|---|---|
| **journey 家族前导 `%%{init}%%` 惰性指令**（journey 4 + timeline 5） | **9** | 把 journey-family 门里既有的“见 `%%{` 且含 theme 即回退”扫描改为结构化指令准入；footer 复用 | **选中** |
| timeline `disableMulticolor` 行 | 1 | 指令含内容键 `timeline.disableMulticolor` | 排除：改变绘制内容 |
| timeline 多行 `themeVariables` 行 | 1 | 嵌套映射覆写颜色 | 排除：绘制内容 |
| state v1/v2 `%%{init}%%` theme 行 | 8 | 同形机制，但 body 剥离后不绘（并发区/复合态） | 排除：是 body 债 |
| flowchart 指令行 | 34 | exact-bit 保留面 | 排除 |
| git 多行 `gitGraph`/`themeVariables` 指令 | 9 | 嵌套/多行载荷 | 排除 |
| quadrant 指令行 | 5 | chartWidth/Height/titlePadding/axisPosition 改变几何 | 排除：绘制内容 |

## 2. 选中项结构（实测）

- journey 4 条：单条
  `%%{init: { 'logLevel': 0, 'theme': '<dark|default|forest|neutral>'} }%%`。
  journey 的 frontmatter 主题表只含 redux*，这四个标准调色板只出现在指令里。
  （`description:` 折叠块里的 `%%{init}%%` 是散文，不是正文指令。）
- timeline 5 条：单条
  `%%{init: { 'logLevel': 'debug', 'theme': '<forest|neutral|base|default|dark>'} }%%`。
- 三个渲染器都不读 `theme`/`logLevel`（终端自带 token；logLevel 只影响上游 logger），
  指令在 body 解析时本来就被当作 `%%` 行跳过——准入后的帧 = 去指令 twin 帧 + 页脚。
- twin：synthetic（删除 frontmatter 与所有前导 `%%{init…}%%` 行），9/9 已专项实测可绘。
- 与既有实现对齐：logLevel 采用仓库统一文法（0..5 / trace…fatal，见
  `flow_init_directive`），页脚沿用 `config.logLevel=... -> logger only; not applied`。

## 3. 目标与副作用（预期）

- journey **14/4 → 18/0**；timeline **23/7 → 28/2**；全局 1528/617 → **1537/608**；
  formal 1342 → **1351**；gaps 48、integrity 0；roster 617 → **608**，
  `family_residual:journey` 4 → 0（类别消失）、`family_residual:timeline` 7 → 2
  （类别 41 → 40）。
- 三数预测：①**608** ②**289** ③**9**。

## 4. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

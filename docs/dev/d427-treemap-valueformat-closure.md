# D-427 —— treemap `valueFormat`（6 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「按既有授权继续残余家族」指令。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：D-426 后的体部 census（553 条 fallback，去掉 frontmatter 重放，
  125 条仅配置阻塞）里，treemap 是最大的单家族干净缝隙（16 条中 6 条），且 6 条
  共享一个封闭形状：折叠 description + `config:` > `treemap:` > `valueFormat:`。
- **预期降幅**：6 行（treemap 16 → 10，全局 553 → 547，formal `drawn_at_240`
  1406 → 1412）。
- **判据（语义，D-412）**：这里**不是惰性披露**——treemap 的叶子标签本来就画数值，
  所以源要求的格式必须出现在画面上。文法取自语料实际写法（d3-format 子集）：
  可选 `$`、d3 的无宽度 `0` 零填充标志（分隔符两侧都可能出现，`$0,0` 就是这种
  写法）、可选 `,`、可选 `.N`（0..6）、可选 `%`/`f`；匹配的引号对视为同值。
  其余任何叶、任何键、任何缩进都整源回退。

## 2. 落地内容（单文件 `mermaid_treemap_impl.cpp`）

- **精确十进制字符串运算**：精度四舍五入（含进位）、百分数 ×100、千分位分组
  全部在数字字符串上完成，浮点永不触碰画面文字；语料只含非负普通小数。
- **严格状态机**替换原来的「出现 config 键就 gate」：frontmatter 只允许
  description 载荷 → `config:` → `  treemap:` → `    valueFormat: <spec>`；
  起始 `---` 围栏被跳过。任何越界键/缩进/重复都 gate。
- **风格门不变**：`classDef`/`:::` 仍整源回退（带不带 formatter 都一样）。

## 3. 门与账本

- 新账本 `tools/mermaid/treemap-fmt-d427-cohort.json`（**6 行**）+ 渲染器无关
  生成器 `generate_treemap_fmt_d427_cohort.py`（`--check`），后者含 formatter 的
  逐行 Python 镜像并钉死每条期望标签；digest `0b00a3fa…`、manifest sha
  `c966a725…`、formats 计数 `{'$0,0':2, '.1%':1, '.2f':1, '$.2f':1, '$,.2f':1}`。
- 新门 `tests/ui/test_mermaid_treemap_fmt_d427_cohort.cpp` 3 项：账本钉；6 行
  「绘制 + 每条期望标签逐字出现 + 原始 token 消失（`700000` → `$700,000`）」；
  文法正例（五个 spec 的精确标签、百分数、进位 `9.999 → 10.00`）与负例
  （theme/look、未知叶、`2d`、`$00,0`、`.7f`、未加引号、重复、错缩进、
  无 `config:`、classDef/:::)。
- 矩阵：`treemap_d427_cohort_map()` 分支置于 RFC 0031 treemap cohort 之前，
  规则 38 reach proof。

## 4. 既有断言迁移（本片足迹）

- **RFC 0031 treemap 门**：
  - `StyledAndConfigTreemapsStayWholeSource` 的 `config_excluded` 六行原本断言回退；
    改为「六行必须全部是 D-427 账本成员」（分片关系保留，正例由新门拥有），
    `styled_excluded` 的负例不变。
  - `VisualGateIsKeyBased` 的 `with_config` 负例换成表外叶 `treemap.color: red`
    （仍然 gate），并注明已准入形状归新门。

## 5. 验证（迭代口径）

- 定向：`MermaidTreemapFormatD427Cohort.*` 3/3；`MermaidTreemapCohort.*` 7/7；
  `ctest -R 'Treemap|Charts|Pie'` = **158/158**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-treemap-d427.log{,.exit}`）。
- 矩阵（debug）：`drawn_at_240` **1412**、`source_fallback` **547**、error 0、
  `mandatory_gaps` 48、`integrity_errors` 空、reach proof 通过。
- roster `--emit`：**547 / 38 类**（`family_residual:treemap` 消失），treemap 余下
  10 条全部是 `classdef_styling`；`--self-test` / `--matrix` 绿；E6c 未动。
- release 预设编译（`-Werror`）通过；全量 ctest 与双预设字节比对留到主线收口。
- 三数并列：①绝对 source_fallback **547** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **6**。

## 6. 未验证 / 剩余风险

- 负值、指数、`+`/`-` 符号、宽度/对齐填充不在语料内，文法直接拒绝（整源回退）；
  语料扩展需重审。
- 40 列窄视口下 6 行改为换行绘制（不再回退），已由门逐行钉住。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

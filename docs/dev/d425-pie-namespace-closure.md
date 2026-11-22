# D-425 —— pie 封闭配置命名空间（8 行）

日期 2026-11-22 · 本地，未 push · 承接 D-424 后的 TL「自选家族、不设评审门」授权。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债（含 xychart rotation 3 行）、
ER107 冻结面、10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片证据（先测后选）

- **体部 census（本片新用的选择仪器）**：对全部 **565** 条 fallback 逐行去掉
  frontmatter 后用 `render_mermaid` 重放（临时探针，一个 ~2s 的目标构建 + ~1s 渲染），
  得到 **133 条「仅配置阻塞」**、432 条体部阻塞。分家族：
  flowchart 67、er 9、git 8、**pie 8**、treemap 6、cynefin 5、state-v2 4、
  xychart 4（其中 3 行是冻结的 rotation 历史债，实际可用 ≤1）、mindmap 3、
  radar 3、requirement 3、tree-view 3，其余 ≤2。
- **为什么选 pie**：它是最大的单家族干净缝隙（12 条 fallback 里 8 条仅配置阻塞），
  且 8 条共享一个**封闭命名空间**；pie 已有 RFC 0042 的档位/页脚机制可复用。
  同时测出两个反例并记录：mindmap 的 7 条 `look: neo` 与 sankey 的 17 条
  `family_residual` 在去掉 frontmatter 后**仍然回退**（体部阻塞），故不选。
- **预期降幅**：8 行（pie 12 → 4，全局 565 → 557，formal `drawn_at_240`
  1394 → 1402）。
- **判据（语义，D-412）**：每个叶都是源自身的状态标记，描述终端**无法表达的
  纯像素表现**（高亮、图例位置、甜甜圈孔、标签半径、响应式宽度）或仅日志键，
  因此准入后**画布必须与去 config 的孪生逐行相同**——这是「惰性」的最强形式，
  不是渲染差异代理。仍未准入的 4 条 pie 行的阻塞是体内 `accTitle:`/`accDescr:`，
  需单独的 pie 可访问性 no-op。

## 2. 落地内容（单文件 `mermaid_charts_impl.cpp`）

- 新增 profile `PieNamespace` 与 `pie_namespace_frontmatter()`：要求评审过的折叠
  description（沿用 `xy_description_shape`），`config:` 只允许
  `logLevel`（0…3 / trace…fatal）与 `pie.*` 五个叶；`legendPosition` 封闭集
  `{bottom}`，`donutHole` 有符号小数（越界值与合法值同样惰性），`textPosition`
  保持 `[0,1]`，`useMaxWidth` 布尔，`highlightSlice` 单 token；其余顶层键、
  未知叶、重复、缩进错误、空命名空间一律整源回退。引号标量与裸标量同值
  （`chart_yaml_scalar` 去引号，与 git 的 `git_inert_scalar` 一致），页脚披露
  **归一后**的值。
- **派发顺序**：RFC 0042 主题 profile 在前（5 条已绘制行帧逐字节不变）→ 新
  namespace profile → 旧的 `pie_config` 守卫 → 纯 description。因此
  `theme:`+namespace 的组合会整源回退，而不是静默丢 theme。
- **页脚**：复用 `Pie native projection v1` 头（抽出 `pie_notice_header()`），
  先 `config.logLevel=… -> logger only; not applied`，再按固定顺序输出各叶。

## 3. 门与账本

- 新账本 `tools/mermaid/pie-ns-d425-cohort.json`（**8 行**）+ 渲染器无关生成器
  `generate_pie_ns_d425_cohort.py`（`--check` 隐式门）。钉：cohort digest
  `1a8a8a66…`、manifest sha `c966a725…`、counts（donutHole 3、highlightSlice 2、
  legendPosition/textPosition/useMaxWidth/logLevel 各 1）。页脚行由 `leaves`
  在 C++ 侧重建并与账本逐行比对。
- 新门 `tests/ui/test_mermaid_pie_ns_d425_cohort.cpp` 3 项：账本钉与摘要；
  8 行「绘制 + native 帧 + 40 列回退 + 页脚精确 + **画布 = 去 config 孪生画布**」；
  命名空间值/形状负例 9 条（未知叶、`side`、`1.4`、`yes`、重复、缩进、`logLevel: 9`、
  theme+namespace 组合、`pie: {}`）与正例 8 条，另加引号标量正例。
- 矩阵：`pie_d425_cohort_map()` + 分支置于 RFC 0042 pie 分支**之前**，post-loop
  断言「到达集合 == 账本集合」（规则 38）。

## 4. 既有断言迁移（本片足迹）

- **Pie7 补充控制翻正**：`MermaidPie7.SupplementarySourceControlsCloseEnvelopeAndSubcellBoundaries`
  的 `config_root_is_valid_unsupported` 控制源原本是
  `description + config.pie.textPosition: 0.9`（那时该形状必须整源回退）；本片起它被
  准入，故按「只加不减」换成表外叶 `pie.showValues: true`（仍整源回退，sha 重钉
  `07c930c9…`），准入侧由新门的 `textPosition` 正例覆盖。计数 5/6/1/3 不变。
- roster 与矩阵人口不变；E2a/E2b 扫描不覆盖 pie。

## 4. 验证（收口实测）

- 定向：`MermaidPieNamespaceD425Cohort.*` 3/3；官方矩阵含两条 reach proof 通过。
- 矩阵（debug）：`drawn_at_240` **1402**（1394 + 8）、`source_fallback` 557、
  error 0、`mandatory_gaps` 48、`integrity_errors` 空。
- 双预设矩阵**逐字节相同**：sha256
  `b2da5099f13051c2d6fddd1850836933be9648b8b8a934c7114541446ac75e36`
  （`matrix-d425-debug3.json` / `matrix-d425-release3.json`）。
- `ctest -L mermaid`（run2，run1 因 Pie7 控制源未迁移而红）：**1523/1523 通过**，
  `CTEST_EXIT=0`，日志 `/tmp/loom-logs/ctest-mermaid-d425-run2.log`，退出码落盘
  `.log.exit`。
- roster `--emit`：**557 / 40 类**，`family_residual:pie` **12 → 4**，其余 39 类
  零漂移；`--self-test` 与 `--matrix` 绿；E6c 冻结缺口 roster 未动。
- **全量 debug ctest**：`100% tests passed out of 4708`，**`CTEST_EXIT=0`**
  （退出码落盘 `/tmp/loom-logs/ctest-full-dbg-d425.log.exit`，日志
  `/tmp/loom-logs/ctest-full-dbg-d425.log`，sha256 `5efc7925…`；4708 = 4704 + 4
  项新门）。无红项。
- 三数并列：①绝对 source_fallback **557** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **8**。

## 5. 未验证 / 剩余风险

- `highlightSlice`/`legendPosition`/`donutHole`/`textPosition` 在终端**不生效**，
  只披露；这是与 look/theme 相同的已声明惰性契约，判据是孪生画布逐行相等。
- `donutHole` 的越界值（-0.3 / 1.2）与合法值同样惰性：上游的「优雅处理」在终端
  投影中无对应物，页脚只披露取值。
- 4 条 pie theme 行（dark/default/forest/neutral）仍回退，等待 pie 的
  `accTitle:`/`accDescr:` no-op；这是已识别的下一小步，不是本片遗漏。

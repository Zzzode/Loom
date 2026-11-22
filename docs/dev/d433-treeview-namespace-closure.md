# D-433 —— tree-view 封闭 `config` 命名空间（2 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「按普查表取下一族，一片一提交」指令。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：D-428 体部 census 之后，agentflow 的 2 条是 `look: handDrawn`
  （与既有 43 类同墙，不可取），venn/ishikawa 同理；tree-view 还剩 4 条，其中 2 条
  只卡一个封闭 frontmatter 形状（`config.treeView.showIcons` 或
  `defaultIconPack`），是本轮最干净的一片。
- **预期降幅**：2 行（tree-view fallback 6 → 4；roster 桶 `family_residual:tree-view`
  4 → 2，全局 536 → 534，formal `drawn_at_240` 1423 → 1425）。
- **判据（语义，D-412）**：终端投影逐行转写节点、不画图标/颜色/线宽/像素缩进，
  所以这些叶惰性且必须披露（`TreeView native projection v1`）；画面文字与 `config`
  剥离后的孪生逐行相等。`showIcons` 与 `defaultIconPack` 是源自身状态标记，不是
  渲染差异代理量。

## 2. 落地内容（单文件 `mermaid_treeview_impl.cpp`）

- `tree_namespace()` 是三态解析：`accepted=false`（拒绝，整源回退）、
  `accepted=true` 且无叶（普通源 / 仅 description 的 frontmatter / 剥离 config 的
  孪生）、`accepted=true` + 叶（绘制并在尾部披露）。
- 缩进栈文法（不依赖固定列宽，语料里 `treeView:` 的子叶缩进既有 4 也有 6）：
  顶层仅 `description:` 与 `config:`；`config` 下仅 `treeView:` / `themeVariables:`
  两个映射；`themeVariables` 下仅 `treeView:`；叶只允许
  `config.treeView.{showIcons, defaultIconPack, rowIndent, lineThickness}` 与
  `config.themeVariables.treeView.{labelFontSize, labelColor, lineColor}`；
  重复映射/重复叶/未知叶/未知键/非法值一律拒绝。
- 值校验：`showIcons` ∈ {true,false}；`defaultIconPack` 小写词；
  `rowIndent`/`lineThickness` 正整数 ≤ 99999 且无前导零；`labelFontSize` 为
  `<digits>px`（可带引号对）；颜色为 `#RRGGBB`（可带引号对）。
- 披露顺序固定（与源顺序无关），每叶一行 "not applied" 说明。
- 体部门不变：`classDef`/`:::`/`style`/`%%{init}%%` 仍整源回退。

## 3. 门与账本

- 新账本 `tools/mermaid/treeview-ns-d433-cohort.json`（**2 行**）+ 渲染器无关生成器
  `generate_treeview_ns_d433_cohort.py`（`--check`），digest `91c436b5…`、manifest
  sha `c966a725…`；生成器另镜像两条「不可准入」的体部/围栏事实（空 `icon()`
  实参、`---      ` 闭合围栏）。
- 新门 `tests/ui/test_mermaid_treeview_ns_d433_cohort.cpp` 3 项：账本钉；
  2 行「绘制 + 原样画面 == config 剥离孪生 + 脚注逐字」；封闭命名空间正例
  （各叶单独 + 五叶合法的合成形状，固定顺序脚注）与负例（`theme`/`look`、未知叶、
  `showIcons: yes`、`rowIndent: 0`、`lineThickness: 1.5`、重复叶、重复映射、叶直接
  挂在 `config`、嵌套 `filenameIcons`、坏颜色/字号、体部 classDef/:::、
  `%%{init}%%`，以及 `---      ` 围栏的可达性边界）。
- 矩阵：`treeview_ns_d433_cohort_map()` 分支置于 RFC 0031 tree-view cohort 之前，
  规则 38 reach proof。

## 4. 既有断言迁移（本片足迹）

- **RFC 0031 tree-view 门** `GatedTreeviewsStayWholeSource`：`frontmatter_gated`
  的 5 行改为「D-433 账本成员恰好 2 行（正例由新门拥有），其余 3 行仍整源回退」；
  `body_style_gated` 两行不变。
- `VisualGateIsKeyBased` 的 `config.treeView.rowIndent: 80` 负例换成表外叶
  `config.treeView.colour: red`（仍 gate），并注明已准入形状归新门。

## 5. 验证（迭代口径）

- 定向：`ctest --preset debug -R 'Treeview'` = **11/11**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-treeview-d433.log{,.exit}`）；日志中逐字出现
  `MermaidTreeviewCohort.GatedTreeviewsStayWholeSource`、
  `MermaidTreeviewNamespaceD433Cohort.LeavesAreInertAgainstConfigFreeTwins`、
  `ClosedNamespaceNegativesStayWholeSource`、`GeneratorIsReproducible`，均为
  `Passed`。release 预设同一组门 3/3。
- 矩阵（debug，直接跑 `test_mermaid_official`）：64/64 通过，`drawn_at_240`
  **1425**、error 0、`mandatory_gaps` 48、`integrity_errors` 空、reach proof 通过
  （`/tmp/loom-logs/matrix-d433.log`）。
- roster `--emit`：**534 / 36 类**（`family_residual:tree-view` 4 → 2；tree-view
  总 fallback 6 → 4，row 5 仍在 `icon_font_glyphs` 桶），
  `--self-test` / `--matrix` 绿；E6c 未动。
- release 预设编译（`-Werror`）通过；全量 ctest 与双预设字节比对留到主线收口。
- 三数并列：①绝对 source_fallback **534** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **2**。

## 6. 未验证 / 剩余风险

- **`---      ` 闭合围栏不可达（共享面）**：`classify_mermaid` 的前缀探测要求闭合行
  **恰好** 是 `---`，因此 `tree-view/pr-check/should-render-with-custom-config-and
  -quoted-labels.mmd` 在共享派发层就回退，任何本地解析器都无法准入。要让这行准入
  必须改 `src/ui/visual/mermaid_impl.cpp`（共享派发），不在本片授权内；门里已用负例
  钉住该边界。剩余的 tree-view 行还有 1 条（嵌套 config 映射 `filenameIcons` /
  `extensionIcons`）与 1 条（空 `icon()` 实参体部）。
- 语料里 `lineThickness: 1` 之类未出现；文法按正整数接受，若扩展需重审。
- 40 列窄视口下两行回退（披露行超宽），已由门钉住。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

# D-438 —— sankey 残余族群 NO-GO：整族卡在共享派发表头谓词

日期 2026-11-23 · 本地，未 push · 承接 TL「按普查表取下一族，一片一提交」。

## 1. 结论先行

`fallback_roster.json` 里 `family_residual:sankey` 的 **17 条**残余**全部**用
裸 `sankey` 作体部首行（而非 `sankey-beta`）。共享派发
`detail::classify_mermaid`（`src/ui/visual/mermaid_impl.cpp`，裸首行路径
`bool sankey = header == "sankey-beta";`，frontmatter 窥视路径
`line == "sankey-beta"`）只认 `sankey-beta`，因此这 17 条在进入
`mermaid_sankey_impl.cpp` 之前就被判为「非 sankey 家族」而整体回退。

按当前授权（不碰共享派发），**本族无法做成本地小片**。D-438 实现与验证已完成、
但降幅为 0，故不落地；补丁留档 `/tmp/d438-sankey-config.patch`（1249 行），
等 TL 裁决。

## 2. 已完成的本地工作（可随时应用，不需重做）

在 `mermaid_sankey_impl.cpp` 内实现了封闭 `config.sankey` 命名空间（D-433 /
D-437 的同一模式），并配套生成器、账本、门禁与矩阵分支：

- **准入叶子（惰性 + 披露）**：`useMaxWidth`(true/false)、`width`/`height`
  （正十进制）、`linkColor`(gradient|source|target|`'#rrggbb'`)、
  `labelStyle`(legacy|outlined)、`nodeColors`（`name: '#rrggbb'` 映射，
  重复名拒绝）。披露脚注 `Sankey native projection v1` +
  `config.sankey.<leaf>=<v> -> ...; not applied`。
- **继续拒绝（要兑现语义）**：`nodeAlignment`、`nodeWidth`（几何）、
  `showValues`、`prefix`、`suffix`（改变可见标签文本）；以及未知叶子、
  其他顶层键、`sankey: {}` 之后再挂叶子、重复叶子、缩进错位、体部
  `%%{init}%%`。
- **体部**：`sankey-beta` 与裸 `sankey` 两种首行**逐字转写**（后者当前被共享
  派发拦截，所以单独放宽渲染器不产生任何降幅）。
- **账本**：`tools/mermaid/sankey-config-d438-cohort.json`，11 行
  （17 条残余中 6 条仍停在几何/可见文本叶子上），cohort digest
  `2b0ea4333648be6845490c275c065b914afe865fabcb0f0488bf068a9a7c2639`；
  生成器 `--check` 可复现；矩阵分支带 rule-38 可达集证明。
- **验证**：`test_mermaid_sankey_config_d438_cohort` 在 debug 下
  账本/双孪生/负向三组中，除「所有孪生都因裸 `sankey` 头回退」一项外全绿
  （孪生回退正是本条发现本身）；换成 `sankey-beta` 体的探针全部通过。

## 3. 需要的裁决（共享改动门槛）

- **归属模块**：`src/ui/visual/mermaid_impl.cpp` 的 `detail::classify_mermaid`。
- **提议的共享改动**：两处谓词各加一个别名 ——
  `header == "sankey-beta" || header == "sankey"`（裸首行路径与
  frontmatter 窥视路径必须同步改，否则行为分裂）。
- **本地闭包为何不足**：家族渲染器已能接受（补丁已验证），派发层先拒绝；
  派发是唯一入口。
- **本地替代**：无。除非把 sankey 识别从共享表移出，否则任何本地写法都到不了
  渲染器。
- **影响面与风险**：`sankey` 布尔只喂 `MermaidProbeFamily::Sankey`，不参与其他
  家族判定；但仍属共享派发语义，需矩阵全量 + 双预设字节门验证。
- **最小改动**：上述两行；不改任何既有谓词、不放宽其他家族。
- **验证方案**：应用补丁 + 两行谓词后，期望 11 行由
  `source_fallback` → `drawn`（`family_residual:sankey` 17 → 6），
  `drawn_at_240` 1445 → 1456，矩阵其余字节不变；全量 ctest + 双预设 sha。
- **回滚**：单提交，`git revert` 即回到 514/1445。

## 4. 为什么不是别的片

同轮复核过的小族残余都需要另一类裁决：state（`state-native-routing-debt`，
sha 绑定的路由几何）、timeline（体部 init 嵌套映射）、info（需要作者化版本面板）、
katex/zenuml（外来引擎）、quadrant/venn（轴位置与 style 语句）、gantt
（两条**故意非法日期**的用例，回退本身是正确处置）。sankey 是唯一
「语义惰性、形状封闭」的族群，但被共享谓词整族挡住。

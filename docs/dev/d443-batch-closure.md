# 批边界收口记录 —— D-443（2026-11-23）

本文件是「本批推进到 fallback ≤ 480 或家族全闭环」这一批的批边界证据。全量
ctest 与双预设字节门只在批边界跑一次，日志落盘 `/tmp/loom-logs/closure-d443-*`。

## 1. 本轮净变化

| 项 | 片前 | 片后 |
|---|---|---|
| 全局 source_fallback（wide 240，original track） | 505 | **499** |
| drawn | 1640 | **1646** |
| formal `drawn_at_240` | 1454 | **1460** |
| roster | 505 条 / 35 类 | **499 条 / 35 类** |

- 片提交：`076d1f06`（feat: admit inert look/htmlLabels/layout leaves for three
  families）。三数并列：①**499**；②**289**（未变）；③本批净减少 **6**。
- E6c 48 条、ER107 冻结面、十份受保护文件、已验收产物均未动。
- 用例名：`MermaidD443InertLeavesCohort.LedgerSchemaReviewsAndPins`、
  `.LeavesAreInertAgainstTwins`、`.ClosedLeavesStayWholeSource`、
  `.GeneratorIsReproducible` 全部 `Passed`；迁移后的
  `MermaidRequirementCohort.*`、`MermaidRequirement0037Cohort.*`、
  `MermaidRequirement0044Cohort.*`、`MermaidVennCohort.*`、
  `MermaidIshikawaCohort.*`、`MermaidE2aCohort.*`、`MermaidE2bCohort.*` 全部
  `Passed`；`MermaidCapabilityReport.OfficialMatrix` 通过。

## 2. 全量 ctest（release 预设）

- **4773 条执行，100% passed，退出码 0**（
  `/tmp/loom-logs/closure-d443-release-ctest.log{,.exit}`）；2 条 Disabled、
  既有环境 Skipped（ClipboardImage/E2E_SubagentLive/CompactionLive 等），
  无 Failed。
- 用例名核验：`MermaidD443InertLeavesCohort.LedgerSchemaReviewsAndPins`、
  `.LeavesAreInertAgainstTwins`、`.ClosedLeavesStayWholeSource`、
  `.GeneratorIsReproducible`、`MermaidSmallfam0043Cohort.*`、
  `MermaidCapabilityReport.OfficialMatrix`、
  `MermaidFallbackAttribution.RosterIsReproducibleAndTotal` 全部 `Passed`。

## 3. 双预设字节门

- debug 与 release 的 `mermaid-official-matrix.json` sha256 **逐字节相同**：
  `4f044a42dd7b2a7d2a49e993e648343cb9c472aff6991666169d000f432b83ba`
  （drawn 1646 / fallback 499 / error 0，formal `drawn_at_240` 1460，
  `mandatory_gaps` 48，`integrity_errors` 空）。

## 4. 为什么本批停在 499 而不是 480（测量结论，不是估计）

对 roster 全部 505 行做了「去掉整个 config 块」的 B 型孪生探针（C++，真实
渲染器，宽 240），并在不画的行上追加「删除整个 frontmatter」的 C 型探针，
把 505 行严格三分：

| 类别 | 行数 | 说明 |
|---|---|---|
| A 型：config 块是唯一阻塞（孪生可画） | 61 | 即本地可闭合面 |
| B 型：frontmatter 其他键阻塞（整段删除后即可画） | 27 | 其中 25 行是 flowchart |
| C 型：body 本身不画（渲染特性缺失） | 417 | class/style、note、icon、`%%{init}%%`、shapes 等 |

本片吃掉 A 型 6 行（requirement 3 / ishikawa 1 / venn 2）后，A 型剩余 55 行
的逐条裁决如下（全部已实测，不是推测）：

| 剩余 A 型 | 行数 | 阻塞归属 |
|---|---|---|
| flowchart | 42 | **Flow400 冻结合同**（`flow400_native_whole_source.v1.json`，需重基线授权） |
| er-diagram | 9 | `ER native projection v1` 的 **ASCII 权威面**：unicode/markdown/`<br/>` 标签在投影文法中显式拒绝（"the source authority does not establish arbitrary Unicode/Markdown behavior"） |
| xychart | 4 | 2 行标签旋转 = **三笔历史债之一（不动）**；2 行 `showDataLabel` = 真实数据标签渲染特性 |
| B 型 state | 2 | 1 行体部首部 `%%` 注释（envelope 的 `raw_body.starts_with("stateDiagram")` 边界，语义级合同）；1 行 description 含字面 TAB（description 文法的显式安全拒绝） |

另外，本批实测否掉了两条本想闭合的路线，证据留档：

- **mindmap all-shapes（6 行）**：补齐 bang/cloud/hexagon 三种轮廓后，语法已通，
  但 shaped 布局的 Work 预付费超过上限：画布约 100×135 = 13,500 格 ×
  96 字节/格 ≈ 1.30M，超过 `kWorkLimit = 1,000,000`。实测：整源删掉最后 3 行
  仍不画、删掉最后 6 行仍不画，删掉全部第三层（14 节点）才回到预算内——不是
  小的计费偏差，而是**预算合同**问题（改 `kWorkLimit`/计费公式属于共享合同）。
  该实验性 shape 语法改动已整体回退，未进入提交。
- **usecase（18 行）**：16 行还带 note/json/style/classDef/icon 等 body 特性；
  唯一只差 `accTitle`/`accDescr` 的 1 行还缺 description（该 fixture 无
  `description`，而 envelope 合同要求 description 先行）。

按实测：本片之后**没有任何一行可以在不改共享合同的前提下闭合**。要走到 480
至少还需要 19 行，唯一可能来源是下列合同级决定之一（均超出本里程碑授权）：

1. Flow400 重基线（flowchart 42 行 A 型 + 更多 B/C 型）；
2. sankey 裸头派发别名（17 行，补丁在 `/tmp/d438-sankey-config.patch`）；
3. swimlanes 家族渲染器（46 行，`src/` 中没有任何 swimlane 实现）；
4. 体部 `%%{init}%%` 披露契约（git 8 + quadrant 5 + timeline 2）；
5. `---` 围栏严格化（2 行）；
6. mindmap shaped 预算（6 行，需显式放宽计费/上限）；
7. ER 投影 unicode 权威面（5-9 行）。

## 5. 负向迁移（同批完成）

`test_mermaid_requirement_cohort.cpp`、`test_mermaid_requirement_0037_cohort.cpp`、
`test_mermaid_requirement_0044_cohort.cpp` 中三处把
`config.flowchart.htmlLabels` 钉为「必须整源回退」，迁为「由 D-443 门禁负责的
已画行」，负例重指到仍被拒绝的 `flowchart.curve` 形状；
`test_mermaid_venn_cohort.cpp` / `test_mermaid_ishikawa_cohort.cpp` 的门禁分区
改为 `d443` 桶（venn frontmatter 3→0、body_style 4→5、d443 2；ishikawa
frontmatter 1→0、d443 1），分别有对应生成器的 `--check` 守护。

## 6. 评审与遗留

- 独立对抗性评审（fresh general-purpose agent，只读）：**APPROVE**。两处 MINOR
  已在同批修复（ishikawa 的四个 `look` 负例原先跑在 venn body 上，已改为
  ishikawa body 直测；本文件的迁移措辞已按实际 diff 改写），两处 NIT 中
  `_seed_ok` 的 Unicode 数字问题已修（`isascii()`），剩余一处 NIT 记录在此：
  `requirement-cohort.json` / `requirement-f0-cohort.json` /
  `requirement-0037-cohort.json` 的 `selection` 列表仍把 3 条已画行列为
  gated/deferred/excluded；它们不被任何门禁读取（官方矩阵只读 `rows`），属
  读者误导级的陈旧账本，留待下次触碰这三个生成器时一并重指。

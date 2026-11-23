# 批边界收口记录 —— D-443…D-449（2026-11-23，第二轮）

本文件是「本批推进到 fallback ≤ 480 或家族全闭环」在第二轮连续推进后的批边界证据。
全量 ctest 与双预设字节门只在批边界跑一次，日志落盘
`/tmp/loom-logs/closure-d449-*`。

## 1. 本轮净变化与切片

| 片 | 行数 | 说明 | 提交 |
|---|---|---|---|
| D-443 | 6 | requirement `config.flowchart.htmlLabels`、ishikawa `look`、venn `look`/`handDrawnSeed`/`fontFamily` | `076d1f06` |
| D-445 | 2 | c4 封闭 `UpdateLayoutConfig` 调用（大纲投影，无几何） | `7c55d30c` |
| D-446 | 1 | state 体部首部 `%%` 注释行（envelope 头部检查） | `edb79a3f` |
| D-447 | 1 | usecase `config.usecase` 九叶 + `accTitle`/块 `accDescr` no-op | `68af527e` |
| D-448 | 1 | xychart 负值/小数柱数据标签（纵向：柱尖对称、一格柱外侧；横向 profile 待认证） | `e6ed3ea9` |
| D-449 | 4 | state `state X: Description` 声明形式 | `ca0efdee` |

| 项 | 批前 | 批后 |
|---|---|---|
| source_fallback（wide 240） | 505 | **490** |
| drawn | 1640 | **1655** |
| formal `drawn_at_240` | 1454 | **1469** |
| roster | 505 / 35 类 | **490 / 35 类** |

三数并列：①**490**；②**289**（未变）；③净减少 **15**。
辅助提交：`341908cf`（D-443 smallfam 迁移）、`cf1defde`（评审修整）、
`df96bd63`/`20b25704`/`14803c05`/`21db562c`（批边界对抗性复核四条修整）、
`fbfdd5cd`（横向 xychart 行按受保护 E6c 观测保持回退，2 行→1 行）、
`48747da4`（五个家族普查门的跳过集迁移）。

## 2. 每片测例名（均核到用例名）

- `MermaidD443InertLeavesCohort.{LedgerSchemaReviewsAndPins,LeavesAreInertAgainstTwins,ClosedLeavesStayWholeSource,GeneratorIsReproducible}`
- `MermaidC4LayoutConfigD445Cohort.{LedgerSchemaReviewsAndPins,CallIsInertAgainstCallFreeTwins,ClosedCallsStayWholeSource,GeneratorIsReproducible}`
- `MermaidStateLeadingCommentD446Cohort.{LedgerSchemaReviewsAndPins,CommentsAreInertAgainstTwins,OnlyLeadingCommentsAreSkipped,GeneratorIsReproducible}`
- `MermaidUsecaseAccessibilityD447Cohort.{LedgerSchemaReviewsAndPins,LeavesAndAccAreInertAgainstTwins,ClosedNamespaceAndAccNegatives,GeneratorIsReproducible}`
- `MermaidXychartNegativeLabelsD448Cohort.{LedgerSchemaReviewsAndPins,EveryValueLabelsExactlyOnce,ClosedProfileNegatives,GeneratorIsReproducible}`
- `MermaidStateDescriptionD449Cohort.{LedgerSchemaReviewsAndPins,DescriptionsAreInertAgainstTwins,DescriptionDeclarationShape,GeneratorIsReproducible}`
- 迁移（批边界复核后补齐）：`MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource`
  （45→40）、`MermaidE2bCohort.NonWhitelistedConfigRowsKeepWholeSource`（22→17）、
  `MermaidC4Cohort.StyledC4sStayWholeSource`（gated 1→3）、
  `MermaidUsecaseCohort.GatedUsecasesStayWholeSource`（+1）、
  `MermaidXychart0035Cohort.SignedValueCapabilityRowsPinOrientationProfiles`
  （纵向转正例、横向保持回退）；`MermaidCapabilityReport.MermanXY102` 保持
  90/12/8/12 原钉不变。
- 迁移（片内已完成）：`MermaidSmallfam0043Cohort.*`（10 条）、`MermaidRequirement*Cohort.*`、`MermaidVennCohort.*`、`MermaidIshikawaCohort.*`、`MermaidMindmapTidyD439Cohort.*`

## 3. 全量 ctest（release 预设）

- `cmake --preset release` + `cmake --build --preset release -j8` 均成功
  （`build=0`，日志 `closure5-release-{cfg,build}.log`）。
- `ctest --test-dir build/release -j8`：退出码 **0**；CTest 收集 **4790** 个
  用例，执行 **4788**（2 个 `Disabled` 不计入分母），**100% passed**
  （`4788/4788`），`Total Test time (real) = 128.40 sec`；日志
  `/tmp/loom-logs/closure5-release-ctest.log{,.exit}`。
- 25 个 `Skipped` 为本机平台性跳过（Windows/macOS 专属、PTY、子代理实连），
  与门禁无关；无 `Failed`、无 `Timeout`、无 `NOT_BUILT`。
- 第一轮 release 运行（`closure-d449-release-ctest.log`）**作废**：`build.ninja`
  仍引用已删除的临时探针目标，`build=1` 后产生 6 个 `NOT_BUILT`
  （5 个新片门禁 + 探针本身），其余 4773 个已建用例全过；重新 configure 后
  才得到上表的有效结果。教训已记入 `docs/dev/engineering-efficiency.md`。

## 4. 双预设字节门

- `build/debug/tests/ui/mermaid-official-matrix.json` 与
  `build/release/tests/ui/mermaid-official-matrix.json` 的 sha256 均为
  `5ae1aea45a88d9ec2e13e3371b570cef1b2418e814de4204718cd6caf8fa5d31`
  —— **字节相同**。
- 两侧口径一致：1655 drawn / **490** source_fallback / 0 error
  （wide 240 与 restored 240 相同；narrow 40 为 641/1504）、formal
  `drawn_at_240` = **1469**、`mandatory_gaps` = 48、`integrity_errors` = 空。

## 5. 为什么仍停在 490（第二轮实测，全部可复现）

每片都先用临时 C++ 探针（真实渲染器，宽 240）测出确切阻塞面，再决定做/不做：

| 剩余面 | 行数 | 实测结论 |
|---|---|---|
| flowchart | 240 | **Flow400 冻结**（受保护合同） |
| swimlanes | 46 | `src/` 无任何 swimlane 实现（新族渲染器，RFC 级） |
| sankey | 17 | 共享 `classify_mermaid` 只认 `sankey-beta`（补丁在 `/tmp/d438-sankey-config.patch`） |
| git | 20 | `>8 branch` 容量 + `%%{init}%%` 体部披露契约 |
| class-diagram | 25 | click（permanent）+ classDef/CSS 投影特性 |
| er-diagram | 24 | `ER native projection v1` ASCII 权威面 + style 语句 |
| state-diagram(-v2) 其余 | 21 | **relations 布局容量墙**：实测扇出 4 转移可画、第 5 条数据转移即回退（共享几何契约）；其余为 classDef/click/note 特性 |
| usecase 其余 | 17 | note 之外的 `json` 块 / `@{...}` 属性 / style（渲染特性） |
| treemap / quadrant / venn / c4 其余 | 28 | classDef/style 与 `%%{init}%%` 体部契约 |
| xychart 其余 | 3 | 标签旋转三债（**约定不动**）+ 横向负值标签 profile 待认证（E6c 观测钉住） |
| tree-view / katex / info / zenuml / gantt / conf | 15 | 注解、Flow400 内嵌、新面、无效日期（回退正确） |

结论：**490 之后没有任何一行能在不改共享合同/不建新族渲染器的前提下闭合**；
距离 480 还差 10 行，全部落在上表的合同级面（含受保护 E6c 花名册钉住的
横向 profile）。满足停止条件 ①。

### 5.1 受保护 E6c 观测冲突（横向 xychart 一行）

D-448 的横向孪生行随同一个 profile 修复本可转正，但 E6c merman_xy102
花名册以 `source_fallback` 钉住该观测（`merman/f5f012ca4e7055701b83`），
而该花名册是本里程碑的十份受保护文件之一。因此横向行保持整源回退，
D-448 只闭合纵向 1 行，`MermaidCapabilityReport.MermanXY102` 维持
90/12/8/12 原钉不变。若 TL 授权一次 E6c 花名册对账（把该观测改为
`candidate_art`），可再闭合该 1 行（fallback 490 → 489）。

## 6. 评审

- D-443：独立对抗性评审 **APPROVE**，2 MINOR + 1 NIT 已修、1 NIT 记录在案。
- D-445…D-449：批边界统一跑一次独立对抗性复核，**REQUEST-CHANGES**，
  四条意见全部落地（无驳回）：
  - D-448 MINOR：`tiny_span` 重定向把**原本已可画**的横向一格柱标签从"绘图区
    右侧外"挪到了柱尖——无 fixture 覆盖该形状，所以矩阵字节保持不变而任意用户
    输入的绘制结果变了。修法：重定向限定纵向（`!chart.horizontal`）；新增横向
    一格柱正例，钉住"两个一格柱的标签同列且与柱尖之间留空"。
  - D-447 MINOR：标量 `accTitle:`/`accDescr:` 的值取自 `blank_quotes` 之后的
    形状视图，规范拼写 `accTitle: "..."` 被误拒（且 `usecase_accessibility_value`
    的引号剥离分支因此不可达）。修法：键仍走形状视图，值改读原始语句；新增
    引号拼写正例。
  - D-445 NIT：`UpdateLayoutConfig` 未限定顶层。修法：加 `depth != 0` 拒绝，
    新增"块内调用"负例。
  - D-449 NIT：重复 `state X: desc` 的语义未钉住。修法：新增重复声明正例
    （两段描述都渲染）与"已声明状态再附描述"正例。
- 修整后由同一独立评审 agent 复审：**APPROVE**（无剩余 MAJOR/MINOR；四条意见
  全部确认关闭，五份生成器 `--check` 全绿，受保护文件零改动）。
- 复核确认项：D-448 外侧标签只落在空单元（碰撞即整源回退）、零值柱仍回退；
  D-446 跳过按 Work 计费且只认列首 `%%`；D-447 前端封闭表/重复/无叶命名空间
  全部拒绝、块体受 `kUsecaseMaxStatements` 限制；五份账本 `--check` 全绿、
  规则 38 可达集已证、十个受保护文件零改动、Flow400 未触碰。

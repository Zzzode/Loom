# 批边界收口记录 —— D-439 / D-440 / D-441（2026-11-23）

本文件是「继续按普查推进下一族」这一批的批边界验证证据。全量 ctest 与双预设
字节门按 TL 口径只在批边界跑一次，日志独立落盘（`/tmp/loom-logs/closure-d440-*`）。

## 1. 本轮净变化

| 片 | 提交 | 行数 | fallback | formal `drawn_at_240` |
|---|---|---|---|---|
| 起点（D-435..D-437 收口后） | `d5a5b9ac` | — | 514 | 1445 |
| D-439 mindmap 封闭 `config.layout: tidy-tree` | `1651284e` | 3 | 511 | 1448 |
| D-440 flowchart 封闭 `config.theme: forest` | `f36aaa3d` | 1 | 510 | 1449 |
| D-441 state 封闭 `config.layout: elk` | `f82993ff` | 3 | **507** | **1452** |

roster：**507 条 / 35 类**；`--self-test` 绿；E6c 48 条未动。

三数并列：①绝对 source_fallback **507**；②超出认证 profile/冻结权威 **289**
（未变）；③本轮净减少 **7**（514 → 507）。

## 2. 全量 ctest（release 预设）

- `cmake --build --preset release -j8` 退出码 0 → `ctest --preset release -j8`
  （带 OpenSSL `LD_LIBRARY_PATH`）。
- **4765 条，100% passed，退出码 0**（
  `/tmp/loom-logs/closure-d441-release-ctest2.log{,.exit}`）；2 条 Disabled、
  25 条 Skipped（既有环境原因），无 Failed。
- **收口时抓到并修复的 3 处门禁回归（D-441 的负向迁移债）**：
  `MermaidState0038Cohort.LookTableIsClosed` 的 `layout: elk` 负例（迁移到
  `layout: dagre`）、`MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource`
  （`checked` 48 → 45）与 `MermaidE2bCohort.NonWhitelistedConfigRowsKeepWholeSource`
  （`checked` 25 → 22，两个跳过集并入 D-441 账本）。修复后全量首跑即 100%。
- 用例名核验（均在通过日志内）：D-439 的 3 条 + 3 条 Generator、
  D-440 的 3 条 + 3 条 Generator、`MermaidFallbackAttribution.*`、
  `MermaidCapabilityReport.OfficialMatrix` 全部 `Passed`。

## 3. 双预设字节门

- debug 全量重编（退出码 0）后单独跑 `MermaidCapabilityReport.OfficialMatrix`；
  debug 与 release 的 `mermaid-official-matrix.json` sha256 **逐字节相同**：
  `31055ad77a5f0f5942ac810d94df743aff50916f0e931655e443e79bfc48ed60`
  （drawn 1638 / fallback 507 / error 0，`drawn_at_240` 1452，
  `mandatory_gaps` 48，`integrity_errors` 空）。

## 4. 本批最重要的结构性发现：Flow400 合同冻结 flowchart 全族

- **实测**：把 flowchart `config.theme` 的封闭表取五个 redux 系调色板落地后，
  debug 矩阵一次即报 **`flows.errors = 8`**、`mandatory_gaps 48 → 57`，
  8 条 `flowchart/nightly-check/redux-color-subgraphs/*` 全部
  `flow_whole_null_unmet`。
- **根因**：`flowchart/` 目录下的 **400 条 fixture 恰好就是冻结的 Flow400 合同
  全集**（400 源、8 个 complete model、392 个 pending/null）。pending 行在构造
  model、重算受审 SHA 之前不许被渲染器画出。
- **结论**：flowchart 侧所有整源回退（look/theme/htmlLabels/spacing/形状等约
  80+ 行）**不是配置切片**，只能通过 Flow400 重基线推进——与 ER107 join、
  Block58 join 同型的受保护里程碑，需 TL 裁决。本批只吃掉合同外的那 1 行。

## 5. 待裁决的岔路（累计）

1. **Flow400 重基线**（flowchart 全族 ~80+ 行；本批新增，证据见 §4）。
2. **共享派发裸 `sankey` 别名**（17 行；补丁 `/tmp/d438-sankey-config.patch`）。
3. **共享 `classify_mermaid` 的 `---` 闭合围栏**（2 行）。
4. **体部 `%%{init}%%` 披露契约**（git 8、quadrant 5、timeline 2 等；已有 78 条
   已画帧携带静默 init，补披露会改其字节）。
5. **git >8 分支容量**（4 行）。
6. **xychart 退化定义域/点标签**（1 + 4 行）。
7. **state 路由几何**（`state-native-routing-debt`，sha 绑定评审集）、
   **classdef/style 语句**（37 + venn 4 + er 1 + c4 6）、
   **info 面板 / katex / zenuml** 新渲染面、**gantt 两条无效日期**（回退正确）。

## 6. 未触碰的契约

十个受保护文件、ER107 冻结面、Flow400 合同、E6c 48 条、排除 43 条、
flowchart exact-bit 222 均未改动；两片各自独立提交，可单独 revert。

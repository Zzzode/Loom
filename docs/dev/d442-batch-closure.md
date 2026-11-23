# 批边界收口记录 —— D-442（agentflow 家族闭合，2026-11-23）

本文件是「下一批至少闭合一个家族」这一批的批边界验证证据。全量 ctest 与双预设
字节门只在批边界跑一次，日志落盘 `/tmp/loom-logs/closure-d442-*`。

## 1. 本轮净变化（家族闭合）

| 项 | 片前 | 片后 |
|---|---|---|
| agentflow 族 wide-240 回退 | 2 | **0**（38/38 原始 fixture 全部 drawn） |
| 全局 source_fallback | 507 | **505** |
| formal `drawn_at_240` | 1452 | **1454** |

- 片提交：`007c3ea3`（feat: close the agentflow family with the inert
  handDrawn look and seed）。
- roster：**505 条 / 35 类**，`--self-test` 绿；E6c 48 条未动。
- 三数并列：①**505**；②**289**（未变）；③本批净减少 **2**。

## 2. 全量 ctest（release 预设）

- **4769 条，100% passed，退出码 0**
  （`/tmp/loom-logs/closure-d442-release-ctest.log{,.exit}`）；2 条 Disabled、
  25 条 Skipped（既有环境），无 Failed。
- 用例名核验：`MermaidAgentflowHanddrawnD442Cohort` 4 条（含
  `GeneratorIsReproducible`）、`MermaidAgentflow0041Cohort.*`、
  `MermaidAgentflowCohort.*`、`MermaidCapabilityReport.OfficialMatrix`、
  `MermaidFallbackAttribution.RosterIsReproducibleAndTotal` 全部 `Passed`。

## 3. 双预设字节门

- debug 与 release 的 `mermaid-official-matrix.json` sha256 **逐字节相同**：
  `912316fddc30462a9a1dfb269774c33a50e73824576932012c3e4b2fb732b0e4`
  （drawn 1640 / fallback 505 / error 0，`drawn_at_240` 1454，
  `mandatory_gaps` 48，`integrity_errors` 空）。

## 4. 负向迁移（同批完成）

RFC 0041 的两份门禁共三处把 `look: handDrawn` + `handDrawnSeed` 钉为「必须
整源回退」；本片把它们改为「由 D-442 门禁负责的已画行」，其余负例重指到仍被
拒绝的形状（`fontFamily`、`look: neo`、seed `0`、`themeVariables` 映射、体部
init）。全量首跑 100%，无遗留回归。

## 5. 下一批的可选面（均需裁决）

- **Flow400 重基线**（flowchart 全族 ~80 行，受保护合同）；
- **sankey 裸头派发别名**（17 行，补丁就绪）；
- **`---` 围栏**（2 行）；
- **体部 `%%{init}%%` 披露契约**（git 8 + quadrant 5 + timeline 2）；
- 其余为渲染特性（state 路由几何、classdef/style、xychart 标签、git 容量、
  info/katex/zenuml 新面）。

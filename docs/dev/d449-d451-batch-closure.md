# 批边界收口记录 —— D-450…D-451（2026-11-23，第三轮）

本文件是「批目标 fallback ≤ 480」在 D-449 批之后的续推收口。全量 ctest 与
双预设字节门只在批边界跑一次，日志落盘 `/tmp/loom-logs/b1-*`。

## 1. 本轮净变化与切片

| 片 | 行数 | 说明 | 提交 |
|---|---|---|---|
| D-450 | 5 | quadrant 首行 `%%{init: {...}}%%` 闭合 tuning/theme 指令 | `5e6cecf3` |
| D-451 | 6 | c4 `UpdateElementStyle`/`UpdateRelStyle` 闭合属性 + `accTitle`/`accDescr` no-op | `5e6cecf3` |

| 项 | 批前 | 批后 |
|---|---|---|
| source_fallback（wide 240） | 490 | **479** |
| drawn | 1655 | **1666** |
| formal `drawn_at_240` | 1469 | **1480** |
| roster | 490 / 35 类 | **479 / 33 类** |

三数并列：①**479**（已达批目标 ≤480）；②**289**（未变）；③净减少 **11**。

## 2. 每片测例名（均核到用例名）

- `MermaidQuadrantInitD450Cohort.{LedgerSchemaReviewsAndPins,
  InitIsInertAgainstInitFreeTwins, ClosedDirectiveNegatives,
  GeneratorIsReproducible}`
- `MermaidC4StylingD451Cohort.{LedgerSchemaReviewsAndPins,
  StylingIsInertAgainstStylingFreeTwins, ClosedStatementNegatives,
  GeneratorIsReproducible}`
- 迁移：`MermaidQuadrantCohort.{LedgerSchemaReviewCountsAndManifest,
  GatedQuadrantsStayWholeSource, VisualGateIsKeyBased, ...}`（`init_gated`
  5→0）、`MermaidC4Cohort.{LedgerSchemaReviewCountsAndManifest,
  StyledC4sStayWholeSource, VisualGateIsKeyBased, ...}`（`styled_excluded`
  9→0）、`MermaidC4LayoutConfigD445Cohort.ClosedCallsStayWholeSource`（负例
  改点为无属性/缺 `$` 形状）。

## 3. 全量 ctest（release 预设）

- `cmake --preset release` + `cmake --build --preset release -j8` 成功
  （`build=0`）。
- 首次全量运行（`b3-*`）暴露一个 **release-only 悬垂视图缺陷**（见 §6 修整）：
  4796 个用例中 2 个 D-451 门禁失败；修正后重跑为最终证据（`b4-*`）。
- 最终结果见 §5。

## 4. 双预设字节门

- debug 与 release `mermaid-official-matrix.json` 的 sha256 都是
  `9c3f6d1b…`（字节相同）；两侧均为 1666 drawn / **479** source_fallback /
  0 error / formal 1480 / gaps 48 / integrity 0。

## 5. 批边界证据

- 最终 release 全量（`b4-*`，`build=0`）：CTest 收集 **4796** 个用例，
  **4795 通过**；唯一失败 `XaaIdpLoginC20.NoSecretBytesInC1TokenExchangeError`
  是已知的 `-j8` 负载型环境 flake，**单独重跑 0.21 s 通过**；全部 mermaid
  门禁 0 失败（含 12 个 `MermaidCapabilityReport.*`、全部 `Mermaid*Cohort`
  与生成器 `--check`）。`Total Test time = 121.80 s`。
- 双预设矩阵 sha256 均为 `9c3f6d1b107517f1…`，字节相同。
- 日志：`/tmp/loom-logs/b4-release-{build,ctest}.log{,.exit}`、
  `b3-release-ctest.log`（暴露悬垂视图缺陷的首次运行）。

## 6. 评审

- 独立对抗性复核：首轮 **APPROVE** + 3 MINOR / 2 NIT（footer 96 字节上限与
  实际行不符、生成器与 C++ 校验器在空对象/空值/括号值/标识符长度上分歧、
  c4 披露行绕过 `kC4MaxRows`）；**全部修整**（提交 `863ccf70`，折叠进片提交
  前的 `04bbeaa6`/`425782fc` 链），同一评审 agent 复审 **APPROVE**，仅余
  两条已处置 NIT（`$x=""` 已改为拒绝；`styled_excluded` 空循环按人口耗尽
  保留并加钉）。
- release 边界暴露一个 **release-only 悬垂视图缺陷**（`trim_view` 绑定到
  `std::string::substr` 临时量），已修复并记入
  `docs/dev/engineering-efficiency.md`；修复后全量重跑。
- 文档语言：本批收口文档沿用 D-443…D-449 的中文 TL 汇报约定（代码与产品
  文档仍为英文），评审 NIT 已记录在案。

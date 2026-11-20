# Mermaid 全量普查零 fallback 复证记录（D-391 任务 1）

日期 2026-11-20 · 分支 master · HEAD `555f89c1`（复证运行时）· 本地执行，未 push。

## 1. 写死的复证口径（caliber）

| 项 | 固定值 |
|---|---|
| 上游版本 | `mermaid@12.2.0`（`upstream_tag`） |
| 上游提交 | `1fb7869866dd619168d840291c9c54b4f129758b` |
| 样本清单 sha | `c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b9`（A/B 一致） |
| 样本集 | 2738 例 = 2145 original 整源 + 593 body_diagnostic |
| 命令 | `ctest -R '^MermaidCapabilityReport.OfficialMatrix$'`（debug preset，`-j1`） |
| 报告 schema | 1（A/B 一致） |
| 对比基线 A | RFC0033 后 / RFC0034 前，矩阵 sha `c906c74afddc`（`/tmp/d282_gate/matrix-debug.json`） |
| 当前复证 B | 本次新鲜运行，矩阵 sha `ee743515f2e2…`（与 D-383 认证字节相同） |

新鲜运行 rc=0，耗时 246.46s，输出 `build/debug/tests/ui/mermaid-official-matrix.json`，sha256 与已认证基线逐字节相同——实现行为在本里程碑的测试稳定性改动下零漂移。

## 2. 头条总数逐项对比（original 整源轨道）

| 状态 | 指标 | 基线 A (c906c74a) | 复证 B (ee743515) | Δ |
|---|---|---|---|---|
| wide_240 | drawn | 1232 | **1239** | +7 |
| wide_240 | source_fallback | 913 | **906** | −7 |
| wide_240 | error | 0 | 0 | 0 |
| narrow_40 | drawn | 631 | **634** | +3 |
| narrow_40 | source_fallback | 1514 | **1511** | −3 |
| restored_240 | drawn | 1232 | **1239** | +7 |
| restored_240 | source_fallback | 913 | **906** | −7 |
| — | mandatory_gaps | 48 | 48 | 0（冻结） |
| — | integrity_errors | — | 0 | — |

- 整源总数 2145 不变；所有变化方向都是 `source_fallback → drawn`，**fallback 零增长、error 零增长、gap 冻结在 48**。
- body_diagnostic（wide_240）A 与 B 完全相同：drawn 372 / source_fallback 173 / error 48，无变化。

## 3. 图族级逐项对比（37 个图族，仅 1 个有变化）

脚本对全部 37 个 fixture 首段图族做三状态聚合，结果**唯一发生变化的图族是 `er-diagram`**：

```
er-diagram  wide: drawn+7 fallback−7 | narrow: drawn+3 fallback−3 | restored: drawn+7 fallback−7
其余 36 个图族：全部状态零变化
```

## 4. fallback 变化的完整矩阵项归因（7 fixture / 17 case-state）

逐 fixture（original track 内路径唯一，已断言 2145 个互不重复且 A/B 同集合）配对，全部 17 个变化均为单向 `source_fallback → drawn`，无任何反向或新增 fallback：

| Fixture | 变化状态数 | 归因 |
|---|---|---|
| should-render-relationships-between-subgraphs.mmd | 3（全状态） | RFC 0033 group-edge |
| should-render-subgraphs-with-direction-override.mmd | 2（wide/restored） | RFC 0033 group-edge |
| should-render-subgraphs-with-quoted-ids.mmd | 3（全状态） | RFC 0033 group-edge |
| should-render-relationships-between-subgraph-and-entity | 3（全状态） | RFC 0033 group-edge |
| nested-er-subgraphs-with-direction-override-default.mmd | 2（wide/restored） | RFC 0033 group-edge |
| nested-er-subgraphs-with-direction-override-elk-layout.mmd | 2（wide/restored） | RFC 0033 group-edge |
| nested-er-subgraphs-with-direction-override-handdrawn-look.mmd | 2（wide/restored） | **RFC 0034 ER198** |

合计：6 个 RFC0033 group-edge 队列 fixture + 1 个 RFC0034 ordinal-198 fixture；wide/restored 各 +7、narrow +3（narrow_40 下这些 ER 整源按设计仍回退，故部分 fixture 该状态无变化），与头条 Δ 完全对账。

## 5. 结论

- 复证通过：当前实现相对 RFC0033 基线，整源 native 绘制净增 7（1232→1239），全部可归因到已评审、带 owned-proof 的 RFC0033/RFC0034 ER 队列；无任何意外图族漂移、无 fallback/error 增长、无 gap 变化。
- “零 fallback 增长”成立；剩余 906 个整源 fallback 与 48 个强制 gap 是冻结的已知终端能力缺口，受保护 roster 锁定。
- 证据：本记录、`/tmp/d391-mermaid-recert/`（fresh run JUnit/stdout、`family-comparison.txt`、`compare_census.py`、`clean-attribution.txt`）。

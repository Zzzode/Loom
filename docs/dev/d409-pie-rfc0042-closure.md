# D-409 —— RFC 0042（pie look/theme 准入）实现收口记录

日期 2026-11-22 · 分支 master · 起点 HEAD `051cd4d9`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0042 已标记 **implemented**（单阶段完成、
全量 debug ctest 一次完整运行、双预设矩阵字节一致、无受保护文件改动）。

## 1. 家族与选择依据（D-408 / TL 判据）

按「可达 drawn 增量 / 每单位成本」（D-408，复用 D-402 strip 探针并剔除
RFC 0039/0040/0041 已晋升的 52 行，674/674 覆盖）：pie 有 **5 条惰性 theme-only**、
机制面 1 个（charts TU 内新增一个 profile）；flowchart 7 条但 4 条在受保护 `e6c`
gap roster 内、且落在 exact-bit 机器上（RFC 0036 前车之鉴）；ishikawa+venn 合计 4 行 /
2 个机制面。家族结构：24 = 7 已绘制 + **5 theme-only 可达** + 8 `pie.*` 语义 +
4 `accTitle:`/`accDescr:` body 阻塞。

## 2. RFC 生命周期（agent-run 门）

| 门 | 评审 agent | 结论 |
|---|---|---|
| 设计门 r1 | `agent:a91726bc2e6905718`（独立复算，读 charts TU/envelope/Pie7/roster） | REQUEST-CHANGES（1 MAJOR + 5 MINOR + 5 NIT） |
| 设计门 r2 | 同一 agent | APPROVE-TO-ACCEPT（2 新 MINOR 已折入） |
| PRR r1 | `agent:ac41c7a527c6f43f6`（可实施性/false-green/回滚/耦合/先例） | REQUEST-CHANGES → 折入后转 implementable（1 MAJOR + 4 MINOR） |
| P1 代码评审 | `agent:a1874c5c055d0a4af`（只读对抗） | **APPROVE**（0 BLOCKER/MAJOR；2 MINOR + 3 NIT 已改） |

关键修正在落地前完成：
- **设计 r1 MAJOR**：标量规则误述 `chart_yaml_scalar`（它不剥注释、`event.value`
  保留尾注）且「尾注正例」无法通过 → 改为与 `PieConfig` 完全一致（raw 值 + 拒绝引号），
  引号/注释均为反例。
- **设计 r1/PRR MINOR**：`PieTheme` 采用更严的空行/缩进注释拒绝规则；`drawn_at_240`
  记账路径钉死（分支留空 `raw_active_schema` → 通用计数器 1285→1290）；Pie7 summary
  保持 `7 + 17 = 24` 整景算术、把 RFC 0042 写进 scope；生成器分类含取值表成员与
  body 结构规则。
- **PRR MAJOR**：测试计划的 `--emit`/`--matrix` 顺序按字面必红（live 669 vs pinned
  674）→ 固定为 review approved → 全量重建 → 双方矩阵 → cohort gate 绿 → `--emit` →
  `--self-test` → `--matrix`，并同步修正 RFC 0041 的同一句。

## 3. 实现（单个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `a1c5f93e` | `PieTheme` profile + 页脚（`Pie native projection v1`）；5 行晋升；新账本（digest `3caa028a…`，corpus twin 1 + synthetic 4）；6 gtest + 1 hermetic `--check`；矩阵分支（置于 Pie7 特殊分支之后，避免 shadow）；Pie7 summary scope；roster 674→669 |

生产改动仅 `mermaid_charts_impl.cpp` 一个实现单元（`Chart` 增加 TU 内
`notices` 成员、`charts_layout` 签名不变；无模块/导入/BMI 变更）。

## 4. 普查与字节门

| 指标 | 起点（RFC 0041 收口） | 收口（P1） | Δ |
|---|---|---|---|
| pie wide_240 drawn / fallback | 7 / 17 | **12 / 12** | +5 / −5 |
| 全局 wide_240 drawn / fallback | 1471 / 674 | **1476 / 669** | +5 / −5 |
| formal `drawn_at_240` | 1285 | **1290** | +5 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40 drawn（pie 5 行） | — | **0/5** | 0（实测） |
| fallback roster | 674（44 类） | **669（44 类）** | −5 |

矩阵 sha256：debug 与 release 逐字节一致
`b00ab2c34b602c2aff4a2cae17e82d8a26dbad7aca35fa9a503d3d7cc4a0db8c`。
Roster 迁移唯一 diff = 5 条被晋升行（`family_residual:pie` 17→12，类别非空；
`--self-test`、`--matrix` 均绿）。

## 5. narrow_40 与 twin

按规则 34 live 实测：5 行全部 40 列回退（页脚最长行 60 字符 > 40 列视口；十二片
body 的合成 twin 在 40 列可绘，准入后因页脚回退）。twin：`dark` 行与已绘制
`pie/pr-check/simple-sports.mmd` body 逐字节相同（corpus twin），4 条 redux 行无
已绘制等价物（synthetic config-free twin）。4 条 `accTitle:`/`accDescr:` body 阻塞行
作为显式整源负例入 gate。

## 6. 三条债务（未变）

1. xychart rotation 3 行。
2. narrow_40 全回退。
3. ER107 B3 三门。

## 7. 验证

- debug 定向：新门禁 6/6 gtest + 1 hermetic；相邻 guard（charts/pie7/xychart/envelope/
  agentflow/state/git/journey）合计 204/204。
- debug 全量 `ctest -L mermaid`：**1472/1472**，`MERMAID_EXIT=0`（上一轮 1465，
  +7 = 6 gtest + 1 hermetic）。
- release 矩阵：exit 0；debug 与 release 的 `mermaid-official-matrix.json` sha256 相同。
- 全量 debug ctest 一次完整运行：见 §8。

## 8. 全量门与 flake 处置（D-401 §7 纪律）

**唯一一次完整全量运行**（debug 预设，`-j8`，日志
`/tmp/ctest-full-dbg-0042.log`）：

- `100% tests passed out of 4657`，`FULL_CTEST_EXIT=0`，real 544.35 s。
- 无失败项，因此无需 flake 举证或局部重跑；skipped/disabled 项均为既有的平台级
  跳过（Windows/PowerShell、macOS 剪贴板、PTY 不可用环境等），集合与上一轮一致。

未运行 ASan 全量：本轮不触碰 ER107 冻结面与任何接口，且按效率规则 6，巨头冻结门在
ASan 下不可行属于既有的测试架构债（`docs/dev/engineering-efficiency.md`
「Open efficiency work #6」），不在本次家族收口范围内重演。

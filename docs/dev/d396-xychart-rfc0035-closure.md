# D-396 —— RFC 0035（xychart 家族）实现收口记录

日期 2026-11-21 · 分支 master · 起点 HEAD `e2d4fbfe` 之前为 `9b496117`（RFC implementable）
· 本地执行，未 push、未 merge。
范围：RFC 0035 的独立设计/PRR 评审、P1–P4 四阶段实现、xychart 家族普查收口、
OQ-1 业主裁定落地、文档与决策登记。

## 1. RFC 生命周期（agent-run 门）

| 门 | 评审 agent | 结论 |
|---|---|---|
| 设计门 r1 | `agent:ad3f7286f2f4af8be`（独立上下文、自带测量） | REQUEST-CHANGES（1 BLOCKER + 5 MAJOR） |
| 设计门 r2 | 同一 agent 复核修订稿 | APPROVE-TO-ACCEPT |
| PRR prr-r1 | `agent:a3cfba9621c06cef4`（构建/回滚/测试基建视角） | REQUEST-CHANGES（3 MAJOR） |
| PRR prr-r2/r3 | 同一 agent | APPROVE-TO-IMPLEMENTABLE |
| P1 代码评审 | `agent:afdd5fdaf6f59d4dd` | APPROVE（独立选择器复算 17 行、独立渲染探针复算 30/18） |
| P2 代码评审 | `agent:a67ad4e0bf007cffd` | APPROVE（逐行核验双向朝向的逐轴门控与无 reflow） |
| P3 代码评审 | `agent:a56c5abd2821b7b24` | REQUEST-CHANGES → APPROVE |
| P4 代码评审 | `agent:a42d5fffd08111920` | REQUEST-CHANGES → APPROVE |

关键设计修正（均来自评审）：
- **B1（设计 r1）**：`show*: false` 不能只准入不应用——否则画出源码明确隐藏的内容。
  改为实现逐轴抑制（P2），并以「flag=true 孪生」门禁证明只移除该元素。
- **M1/M2（设计 r1）**：可达行数由 26 更正为 27（后经 OQ-1 定为 24）；「三处代码改动」
  扩为完整清单，pie 校验器零改动、用 sibling 校验器。
- **M4（设计 r1）**：E2c ledger 的 reviewed rule 排除 config/init，不可扩展 → 新建
  sibling ledger 与独立测试文件。
- **M3（PRR）**：roster 漂移门不可能「一直绿」除非每阶段同提交 `--emit` 迁移 → 落地为 C9。
- **M1/M2（PRR）**：P1 会翻转 `ConfigTitleAndInitStillFailClosed.with_config`；P5 若重写
  共享 init walker 会翻转 plain-init orientation 断言 → 改为 sibling walker，`xy_init_json`
  零改动。
- **MAJOR-1/2（P4）**：边界集缺口与畸形 init 负例（少一个 `}`）被实证纠正。

## 2. 实现（4 个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `dee2fee5` | 闭合 frontmatter grammar（description 与 config 共存）+ 17 行 |
| P2 | `efa75e8f` | `show*: false` 逐轴抑制（8 个标志、无 reflow）+ 5 行 |
| P3 | `3371f727` | `dataLabelColor` 第 11 个 validated-and-ignored 颜色角色 + 1 行 |
| P4 | `e2d4fbfe` | description+init sibling walker（`plotBorderWidth` 仅 init）+ 1 行 |

每阶段提交都包含：生产代码 + 生成器/ledger + 矩阵分支 + 逐行门禁 + `fallback_roster.json`
（`--emit` 生成，未手改）。生成器带 `--check` 可复现；ledger 采用**逐阶段 review 记录**，
任一已启用阶段未批准则整体 verdict 保持 `pending`（杜绝临时批准假绿）。

## 3. 普查与字节门（写死口径）

| 指标 | 起点（D-395 后） | 收口（P4） | Δ |
|---|---|---|---|
| xychart wide_240 drawn | 13 | **37** | +24 |
| xychart wide_240 fallback | 35 | **11** | −24 |
| 全局 wide_240 drawn | 1247 | **1271** | +24 |
| 全局 wide_240 fallback | 898 | **874** | −24 |
| formal `drawn_at_240` | 1061 | **1085** | +24 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40 drawn（xychart） | 0 | **0** | 0（无承诺） |

阶段矩阵 sha256（debug 与 release 逐阶段字节一致）：

| 阶段 | 矩阵 sha256 | xychart | 全局 | formal |
|---|---|---|---|---|
| P1 | `7047d8a1…` | 30/18 | 1264/881 | 1078 |
| P2 | `5096d1a9…` | 35/13 | 1269/876 | 1083 |
| P3 | `240eec9e…` | 36/12 | 1270/875 | 1084 |
| P4 | `ee75f003…` | 37/11 | 1271/874 | 1085 |

Roster 迁移：906 → 898（D-395 F0）→ 881 → 876 → 875 → **874**（44 类，`--matrix` 与
`--self-test` 均绿），全部经 `fallback_attribution.py --emit`。

## 4. OQ-1 裁定（业主，经 orchestration）

`labelRotation` **不准入**，归入 capability 桶，目标 27 → 24。理由：rotation 改变标签几何，
按未旋转绘制会把未实现的能力记成已绘制（quality laundering）；120/−100 恰好被上游 clamp
到 0° 是巧合，不构成实现。三行保持可见能力债（45°、120°、−100°），实现 rotation 后可
按既有普查口径自然挣回。

## 5. 剩余能力债（11 行，显式 non-goal）

- 逐点内联标签 4 行；带符号柱标签 2 行；零宽轴 1 行；>3 线系列 1 行；label rotation 3 行。
- 这些行在 roster 中保持 `family_residual:xychart`（causality `family-scoped-debt`），
  矩阵保持 `source_fallback`；无静默漂移（未登记晋升会被 `classify()` 记为 error + gap）。

## 6. 验证

- 逐阶段：定向 cohort 门禁（P1 6/6 → P2 8/8 → P3 8/8 → P4 8/8，debug+release）、
  `MermaidXyDescriptionEnvelope` 5/5、`MermaidChartsMermanXY102Resources` 4/4、E2c 5/5、
  生成器 `--check`、roster `--matrix`/`--self-test`。
- 双预设：debug 与 release 全树 `-Werror` 构建通过；每阶段矩阵 sha 字节一致。
- 全量 ctest（第一次，debug，`-j$(nproc)`）：**4614 项，2 失败**——
  1. `MermaidCapabilityReport.MermanXY102`：真实冲突（非 flake）。五个 Merman 源与已晋升
     官方用例语义相同，抑制实现使其变为 `candidate_art`，而受保护 E6c roster 将其钉为
     `source_fallback`。经业主授权后按机械对账更新 roster（见 §7.6），并重跑本测试与
     `verify_e6c_gap_roster.py`（双预设 OK）。
  2. `E2E_Pty_Handoff_HupStress`：环境 flake（全量并发下 2.19s 超时），隔离复跑
     **Passed 12.5s**。
- 全量 ctest（第二次，roster 对账后）：**4614 项，3 失败**——2 项
  `ServerMain.DirectConnect*`（AGENTS.md 载明的 TCP 时序 flake，隔离复跑通过）、1 项
  `MermaidErGroupEdgeAdmission.RejectsUnreviewedOrDriftedCohort`
  （`verify_er_groupedge_admission.py` 的十保护文件表尚未包含本次授权重钉；机械补钉后
  self-test 9/9）。三者隔离复跑全部通过。
- 全量 ctest（第三次，最终）：**4614/4614 通过（EXIT=0）**，含 25 个运行期 SKIP 与
  2 个编译期 DISABLED。
- `rfc_lint.py`：0035 零发现；仓库级红项仅为既有的 0033 重复编号历史遗留（非本轮引入）。

## 7. 未消解 / 推迟项

1. 11 个 capability 行未排期；rotation 需真实几何实现。
2. narrow_40 仍全部回退（家族一致口径，无承诺）。
3. `docs/rfcs/` 的 0033 重复编号遗留使仓库级 lint 红，建议单独清理（不属本 RFC）。
4. B3 / ER107 三门继续不动（等业主确认）。
5. P4 的 orientation 正向证据是「与去 init 孪生帧不同」，未断言水平布局的具体几何
   （该行唯一生效叶即 orientation，评审接受）。
6. **经授权的受保护文件例外（本轮唯一）**：`tools/mermaid/e6c-mandatory-gap-roster.json` 与
   `tests/support/mermaid_gap_roster.hpp`。业主于 2026-11-21 明确授权；变更只含机械对账：
   merman_xy102 五个 `partial_or_hidden_axis_profile_pending` 行（六行中的五行）的
   `observed` 由 `source_fallback` 改为 `candidate_art`（profile 保持 pending/uncertified，
   `certified_*` 仍为 0，20 行与计数不变），并重钉 `kRosterBytes` / `kRosterSha256`
   （最终 45096 bytes / `908b088f…`）。该受保护 diff 由独立 agent
   `agent:ab4df7a2976ef502d` 审查（首轮 REQUEST-CHANGES 仅因本记录缺失与 §7 表述矛盾，
   补齐后放行）。连带机械重钉（非受保护工具）：`tools/mermaid/verify_er_groupedge_admission.py`
   的十保护文件表（roster 45096/`908b088f…`；`mermaid_gap_roster.hpp` 新 sha
   `45012318…`，字节数不变 11591），其 self-test 9/9 通过。其余八个保护文件零改；
   `manifest.json` 及其 sha 级联零改；无 wire/持久化/settings/session 契约变更。

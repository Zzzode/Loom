# D-412 —— RFC 0043（小家族 theme 准入）实现收口记录

日期 2026-11-22 · 分支 master · 起点 HEAD `0c3bc591`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0043 已标记 **implemented**（单阶段完成、
全量 debug ctest 一次完整运行、双预设矩阵字节一致、无受保护文件改动）。

## 1. 三个数（TL 2026-11-22 口径，并列给出，互不替代）

| # | 指标 | 值 |
|---|---|---|
| ① | 绝对 `source_fallback` 总数 | **663**（起点 669） |
| ② | 其中超出已认证 profile / 冻结或受保护权威的条数 | **289** = 222 flowchart exact-bit + 67 protected/frozen（本阶段**未变**） |
| ③ | 本轮净减少 | **6** |

663 = 289 + 374（其余能力债，起点 380 − 6）。按 D-410 的账单子桶：
5a 106 / 5b 15 / 5c 253 / 5d **6 → 0**。

## 2. 家族与选择依据（D-410 / D-411 / TL 指令）

D-410 把 669 条剩余 fallback 按根因分桶后，5d「惰性 theme-only」是唯一可以今天
清零的子类：**6 条**（ishikawa 2、venn 3、railroad 1）。它们都满足：去 frontmatter
后 body 在 240 列原生绘制、仅被一个 `config.theme` 叶子阻塞、三个渲染器各自都有一段
「任意缩进的 theme/look/config 键即整源回退」的键式闸门。

同形状的第 7 条 `conf-and-directives/...settings-from-initialize-nodes...`
（theme forest）需要的是 flowchart 机制，不在本 RFC 范围（仍记在 5c）。

## 3. RFC 生命周期（agent-run 门）

| 门 | 评审 agent 镜头 | 结论 |
|---|---|---|
| 设计门 r1 | `agent:design-review` | REQUEST-CHANGES（2 BLOCKER + 5 MAJOR + 8 MINOR + 3 NIT） |
| 设计门 r2 | 同 agent | REQUEST-CHANGES（2 MAJOR + 4 MINOR + 1 NIT） |
| 设计门 r3 | 同 agent | **APPROVE-TO-ACCEPT**（1 NIT 已折入） |
| PRR | `agent:prr-review`（可实施性/假绿/回滚/耦合/先例） | **APPROVE-TO-IMPLEMENT**（6 MINOR + 2 NIT 已折入） |
| P1 代码评审 | 独立只读对抗（实际运行了门禁与矩阵） | **APPROVE**（0 BLOCKER/MAJOR；3 MINOR + 1 NIT 已改） |

落地前修正的关键事实（都不是文字润色）：

- **5d 是 6 条不是 5 条**：设计 r1 发现漏掉 venn `9-should-render-with-dark-theme.mmd`。
  根因是 D-402 探针日志的 `keys=` 字段把 folded description 的 payload 续行也拼进
  键集——探针谓词的第二次偏差（规则 36）。
- **venn 分区是 11+3+3+4 不是 11+3+4+3**：`explicit-set-style-over-redux-palette.mmd`
  的 `config.theme: redux-color` 被准入后，真正拦它的是 body `style`——生成器必须
  **重分类**而不是减法（设计 r2）。
- **「config 在前、description 在后」是准入而非拒绝**：column-0 `description` 会
  关闭 config 块，其 payload 在块外被忽略（设计 r2，对照 `af_frontmatter_gated`）。

## 4. 实现（单个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `7aa711a3` | 三个 TU 的 `*_scalar` / `*_notice_line` / `*_frontmatter_gated` + 页脚；6 行晋升；三个家族生成器结构化重分类与四路分区；新 sibling 生成器/账本（digest `de0e0ced…`，4 corpus twin + 2 synthetic）；6 gtest + 4 hermetic `--check`；矩阵分支（置于三个家族分支之前，`raw_active_schema` 空）；roster 669→663 |

生产改动仅三个 `.cpp` 实现单元（无模块/导入/BMI 变更，模块图 DAG 不变）。

## 5. 普查与字节门

| 指标 | 起点 | 收口 | Δ |
|---|---|---|---|
| ishikawa wide_240 drawn / fallback | 7 / 6 | **9 / 4** | +2 / −2 |
| venn wide_240 drawn / fallback | 11 / 10 | **14 / 7** | +3 / −3 |
| railroad wide_240 drawn / fallback | 8 / 1 | **9 / 0** | +1 / −1 |
| 全局 wide_240 drawn / fallback | 1476 / 669 | **1482 / 663** | +6 / −6 |
| formal `drawn_at_240` | 1290 | **1296** | +6 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40（6 行） | — | **0/6**（live 实测） | 0 |
| fallback roster | 669（44 类） | **663（43 类）** | −6，`family_residual:railroad` 消失 |

矩阵 sha256：debug 与 release 逐字节一致
`8de4c54617bdc3110cc2a00216f21af5bfca83c5ed6964b882292b129cb58469`。

Roster 迁移唯一 diff = 6 条被晋升行与随之的计数/类别收缩；
`--emit` → `--self-test` → `--matrix` 按唯一绿灯顺序执行，均绿。

## 6. twin、narrow 与 red-first

- **red-first**：改动前在已提交矩阵上实测 6 行在 wide/narrow/restored 三条轨道全部
  `source_fallback`，晋升是实测而非假设。
- **twin**：ishikawa forest/dark 与 `ishikawa/pr-check/1-...` body 逐字节相同；
  venn redux-color/redux-dark-color 与 `venn/pr-check/16-...` 相同（4 个 corpus twin）；
  venn `dark` 与 railroad `dark` 无已绘制等价物 → synthetic（2 个）。
- **narrow**：页脚行 48–60 字符 > 40 列，6 行全部 40 列回退；由 gate 现场断言并
  与账本 `expected_states` 对照（规则 34）。

## 7. 三笔历史债（未变）

1. xychart rotation 3 行。
2. narrow_40 全回退。
3. ER107 B3 三门。

## 8. 验证

- debug 定向：新 gate 6/6；三个家族 cohort 测试 7/6/7；4 个 hermetic `--check` 全绿。
- debug 矩阵：`MermaidCapabilityReport.OfficialMatrix` PASS，1482/663，gaps 48，
  integrity 0，formal 1296。
- release 矩阵：PASS，与 debug sha 相同。
- roster：`--emit` 663/43；`--self-test` 43/43 causality、零未分类；`--matrix` 零漂移。
- **全量 debug ctest 一次完整运行**：`100% tests passed out of 4667`，
  `FULL_CTEST_EXIT=0`，real 545.21 s（日志 `/tmp/ctest-full-dbg-0043.log`）。
  跳过项均为既有的平台级跳过（Windows/PowerShell、macOS 剪贴板、PTY/子代理不可用），
  Disabled 两项为既有禁用；无失败项，因此无需 flake 举证或局部重跑。

未运行 ASan 全量：本轮不触碰 ER107 冻结面与任何接口，且巨头冻结门在 ASan 下属于
既有测试架构债（`docs/dev/engineering-efficiency.md` Open work #6）。

## 9. 开工记录以外的实测修正（已写入规则）

1. 探针 `keys=` 字段会把 description payload 拼进键集 → 一律结构化解析 frontmatter。
2. 旧闸门对**任意缩进**的 `theme/look/config` 键都拒——描述 payload 中「看起来像
   visual key」的行不是文档，是闸门（battery 已按保留语义钉死）。
3. twin 帧组合需保留源自身尾空行（页脚插在 art 与尾空行之间），对尾空行数不敏感。

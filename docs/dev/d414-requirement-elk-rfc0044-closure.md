# D-414 —— RFC 0044（requirement `layout: elk` 准入）实现收口记录

日期 2026-11-22 · 分支 master · 起点 HEAD `e864a051`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0044 已标记 **implemented**（单阶段完成、
全量 debug ctest 一次完整运行、双预设矩阵字节一致、无受保护文件改动）。

## 1. 三个数（TL 2026-11-22 口径，并列给出，互不替代）

| # | 指标 | 值 |
|---|---|---|
| ① | 绝对 `source_fallback` 总数 | **640**（起点 663） |
| ② | 其中超出已认证 profile / 冻结或受保护权威的条数 | **289** = 222 flowchart exact-bit + 67 protected/frozen（本阶段**未变**） |
| ③ | 本轮净减少 | **23** |

640 = 289 + 351（其余能力债）。

## 2. 家族与选择依据（D-413 / TL 指令）

TL 指示从 D-410 桶 5 的 374 条中直接选片、不等口径；排除受保护 43 与 flowchart
exact-bit。D-413 按「文本惰性可证 / 机制成本 / 行数」排序，选中 requirement 的
`config.layout: elk`：24 条落在单一 roster 类别 `elk_layout_engine`，其中 23 条与已绘制
非 ELK 行 body 逐字节相同，且该 TU 已有准入通道与页脚。

## 3. RFC 生命周期（agent-run 门）

| 门 | 评审 agent 镜头 | 结论 |
|---|---|---|
| 设计门 r1 | `agent:design-review` | REQUEST-CHANGES（1 BLOCKER + 1 MAJOR + 3 MINOR + 3 NIT） |
| 设计门 r2 | 同 agent | REQUEST-CHANGES（1 MAJOR + 2 MINOR + 1 NIT，r1 改写引入） |
| 设计门 r3 | 同 agent | **APPROVE-TO-ACCEPT**（1 可选 NIT 已折入） |
| PRR | `agent:prr-review` | **REQUEST-CHANGES → 折入后 implementable**（2 MAJOR + 5 MINOR + 5 NIT） |
| P1 代码评审 | 独立只读对抗（实际运行门禁与两个家族测试） | r1 REQUEST-CHANGES（1 MAJOR）→ **r2 APPROVE** |

落地前修正的关键事实：

- **三处既有断言把 `layout: elk` 钉为回退**（`test_mermaid_requirement_cohort.cpp` 的
  frozen gated 数组与单叶子 battery、`test_mermaid_requirement_0037_cohort.cpp` 的 keys
  battery）——设计 r1 的 BLOCKER。全部改为「准入正例 + `layout: dagre` 负例」，不删断言。
- **剩余 5 条回退的成因**：3 条嵌套 `config.flowchart.htmlLabels` 映射（1 条 ELK、无 twin；
  1 条非 ELK；1 条另带 `look`）、1 条首行空行（门要求 offset 0 为 `---\n`）、1 条闭合围栏
  缩进两格（envelope malformed）。
- **twin 选择规则**：同一 body 可对应多个带不同页脚的已绘制行——优先 footer-free twin
  （20 条），否则 twin 的页脚块必须是源页脚的前缀（3 条，均带 `htmlLabels: false`）。
- **生成器必须是活闸门的镜像**：代码 r1 的 MAJOR——`admits()` 原为 naive split、不检查
  indent-2、不处理引号/注释；已改为 `er_strip_comment` + 仅去空格 + 外层同字符去引号的
  精确镜像，并拒绝 indent≠2/重复/命名空间条目。

## 4. 实现（单个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `1f1debb5` | `requirement_envelope` 增加 `Layout` 位与 indent-2 `layout` 叶（仅 `elk`）+ 页脚行（追加在最后）；23 行晋升；新 sibling 生成器/账本（digest `a3b331ae…`，20 footer-free + 3 footer-prefix）；5 gtest + 3 hermetic `--check`（含新注册的 0037/base 生成器）；矩阵分支（置于 F0/0037/base 三个 requirement 分支之前，`raw_active_schema` 空）；roster 663→640 |

生产改动仅 `mermaid_requirement_impl.cpp` 一个实现单元；`RequirementEnvelope` 已有
`config_notices`，无接口/模块/BMI 变更。未运行 ASan 全量：本轮不触碰 ER107 冻结面与
任何接口，巨头冻结门在 ASan 下的架构债不在本阶段范围内。

## 5. 普查与字节门

| 指标 | 起点 | 收口 | Δ |
|---|---|---|---|
| requirement wide_240 drawn / fallback | 144 / 28 | **167 / 5** | +23 / −23 |
| 全局 wide_240 drawn / fallback | 1482 / 663 | **1505 / 640** | +23 / −23 |
| formal `drawn_at_240` | 1296 | **1319** | +23 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40（23 行） | — | **0/23**（live 实测） | 0 |
| fallback roster | 663（43 类） | **640（43 类）** | −23；`elk_layout_engine` 38→15 |

矩阵 sha256：debug 与 release 逐字节一致
`0732abe2cf53d981cdd1d5d7c8ceaa4c66a0d8109c57f662228917fb97e601ca`。

## 6. twin、narrow 与 red-first

- **red-first**：改动前 23 行在 wide/narrow/restored 三条轨道全部 `source_fallback`。
- **twin**：20 条 footer-free（无 `config:` 块的已绘制 `-default` 行，且都通过 body 逐字节
  校验）；3 条 footer-prefix（唯一已绘制孪生带 `htmlLabels: false` 页脚，其块是源页脚前缀）。
  gate 组帧 = twin 帧去掉 twin 页脚块 + 追加源页脚块；footer-free 时 twin 页脚块为空。
- **narrow**：页脚行 56 字符 > 40 列，23 行全部 40 列回退；gate 现场断言。

## 7. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

## 8. 验证

- debug 定向：新 gate 5/5；两个家族测试 10/10 与 6/6（0037 的 120 行 oracle 135 s）；
  3 个 hermetic `--check` 全绿。
- debug 矩阵：requirement 167/5，全局 1505/640，gaps 48，integrity 0，formal 1319。
- release 矩阵：与 debug sha 相同。
- roster：`--emit` 640/43；`--self-test` 零未分类；`--matrix` 零漂移。
- **全量 debug ctest 一次完整运行**：`100% tests passed out of 4675`、
  `FULL_CTEST_EXIT=0`、real 555.29 s（日志 `/tmp/ctest-full-dbg-0044.log`）。
  跳过/禁用项与上一轮相同，无失败项，因此无需 flake 举证或局部重跑。

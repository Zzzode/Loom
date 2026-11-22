# D-422 —— RFC 0048 收口：State 并发区域 + 体内 `%%{init}%%`/可访问性（25 行）

日期 2026-11-22 · 本地，未 push · 承接 D-421 选片（含其修订 3）与 TL 常驻授权。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、**ER107 冻结面**、10 个受保护文件。

## 1. 落地内容（两个原子阶段）

### P1 —— 并发区域（17 行）

- **State-only 侧表，惰性启用。** `RelationDiagram` 新增
  `state_region_by_entity` / `state_region_count_by_entity`（按实体槽索引）。
  两者只被**第一条 `--`** 启用：无区域文档（含 state-0038 的 28 行）零新增 Work、
  帧与计费字节不变。`entity()`/`control()` 在插入路径写槽；`prepare()` 对
  “表长 ≠ 实体数” fail-closed。
- **只在插入时定区。** `entity()`/`control()` 增加 `int region = 0` 形参，仅在插入
  路径读取；七个 State 创建点传入当前 owner 的游标。`control()` 去重键加入 region，
  每个区域拥有自己的 `[*]` 对。
- **`--` 文法。** 仅限 State `Group` 体内独立 `--` 行；`kStateRegionLimit = 8`；
  顶层 `--`、`A -- B`、空区域、跨区域边、分隔符之后的 `direction` 全部整源回退。
- **分区前 fail-closed。** `prepare()` 对每个 State 作用域用 `scope_child` 把两端点
  映射到直接子节点，区域不等即整源回退；`reach` 越界守卫改为无条件。
- **共享单列 body 的抽取。** `measure()` 仅在“State 且区域数 > 1”时分派到
  `measure_regions()`；后者按区域切分子表与边子集，逐区域调用 `measure_scope()`
  （region 模式返回 `(w,h)`、绝不写 owner 盒）。容器宽 = Σ区域宽 + 每边界 1 列分隔，
  高 = 最高区域（区域高已含唯一 header band）；标题更宽时区域**左对齐**、分隔偏移不变。
- **分隔符几何。** 以容器局部 x 偏移 + 局部首 body 行保存；发射时在 header band 底边
  画 `┬`、body 画 `│`、容器底边画 `┴`，全部在 notes/routing 之前置 blocked
  （重叠 note fail-closed），`ports_for` 排除 junction 单元。
- **增益实测修正。** 范围实测为 **17** 行而非 11：另 6 条
  `concurrency-composite-containers-handdrawn-look-redux-*` 与
  `concurrent-states-neo-look-*` 的唯一阻断就是分隔符（其 `look`/`theme` 叶早已在
  RFC 0038 表内）。见 RFC 0048 "Amendment 1" 与 D-421 修订 3。

### P2 —— 体内指令与可访问性（8 行）

- **信封。** `state_source_envelope` 接受“至多一条**前导**体内 `%%{init}%%`”：
  仅裸 `init`、空 `{}`、`logLevel`/`theme` 扁平对象；`logLevel` ∈
  {0…5, trace, debug, info, warn, error, fatal}，`theme` ∈ {dark, default, forest,
  neutral}。引号状态机、无嵌套、无重复键、CRLF 剥离、尾随文本拒绝（state 本地副本，
  journey helper 不可跨 TU 导入）。命中时 body 使用 `effective_body`。
- **G5 互斥。** frontmatter 视觉叶 + keyed 指令 = 整源回退，因此任何 config 行都
  不会重复输出。
- **页脚。** 在 frontmatter 行之后追加
  `config.logLevel=<v> -> logger only; not applied`、
  `config.theme=<v> -> colour palette; not applied`。
- **可访问性 no-op。** 标量 `accTitle:`/`accDescr:` 在描述/边路径之前被校验消费
  （非空、≤256、无控制字节、unicode、各一次）：它们不会变成状态，也不设置标题；
  block 形式 `accDescr {`、重复、空载荷、裸 `accTitle` 标识符均整源回退。

## 2. 门与账本

- 新账本 `tools/mermaid/state-region-0048-cohort.json`（**25 行**：P1 17、P2 8），
  渲染器无关生成器 `generate_state_region_0048_cohort.py`（`--check` 隐式门）。
  钉：cohort digest `4829fdb1…`、manifest sha `c966a725…`（与矩阵测试常量一致）、
  probes sha `12b09216…`。
- 单源探针 `tools/mermaid/state-init-0048-probes.json`（25 例，9 准入 / 16 回退）
  同时驱动 Python 镜像 `state_init_tables.py` 与 C++ 门（效率规则 37）。
- 逐行 oracle：**帧 = twin + 页脚**，逐行相等。P1 twin = 删 `config:` 块；P2 twin =
  删前导指令行。
- 新门 `tests/ui/test_mermaid_state_region_0048_cohort.cpp` 5 项：
  账本钉/摘要、25 行帧 oracle、12 条文法负例、4 条配置/指令负例、25 例探针。
- 矩阵分支 `RFC-0048 state concurrent-region reviewed cohort` 置于 E2a/E2b/state-0038
  分支**之前**，post-loop 断言“到达集合 == 账本集合”（规则 38）。

## 3. 既有账本/断言迁移（本片足迹）

- E2a：生成器退役 `--` 阻塞器（词表条目保留为封闭集合）；账本 59/10 → **60/9**；
  整源扫描 `checked` 由 `EXPECT_GT(checked, 100)` 重钉为
  `EXPECT_EQ(checked, 80)`（= 104 − 24：16 条 P1 配置行 + 8 条 P2 init 行），
  `skipped_by_rfc0048 == 24`。
- E2b：同上退役；51/10 → **53/8**；整源扫描 `checked == 25`（39 − 14 净减：16 条配置行
  中 2 条本就是 E2b 成员，早已跳过），`skipped_by_rfc0048 == 16`。
- state-0038：账本零迁移；其 `E2bFootersAreUnchanged` 抽取数随 E2b 绘制集 51 → **53**。
- 0046：**实测零迁移**（其计数来自自身 tuning 账本；本片 25 行无 tuning 命名空间），
  套件 5/5 绿。
- 冲突负例翻正 + 配套新负例（断言只加不删）：
  `CompositeDividerStaysSource` → `CompositeDividerDrawsAndRejectsCrossRegion`
  （6 条文法负例 + 三区域 `┬`/`┴` 计数）；
  `InitThemeFrontmatterStaysSource` → `InitThemeDirectiveDrawsAndUnreviewedDirectivesStaySource`
  （3 条指令负例）。
- roster `--emit` **583 / 40 类**；`--self-test` 与 `--matrix` 绿。
  类目：`family_residual:state-diagram` 7 → 2、`:state-diagram-v2` 20 → 6、
  `look_handdrawn` 45 → 43、`look_neo` 32 → 28。

## 4. 验证（收口实测）

- 定向：`MermaidStateRegion0048Cohort.*` 5/5；E2a 5/5；E2b 6/6；state-0038 5/5；
  0046 5/5；relations-semantics `MermaidStateEnvelope.*` 13/13。
- `ctest -L mermaid`：**1514/1514 通过**，`CTEST_EXIT=0`，日志
  `/tmp/loom-logs/ctest-mermaid-0048-final.log`（7273.93 sec*proc，real 301.33 s）。
  （前一版 1515 含临时开发探针 `TempRegionProbe`，已在提交前删除。）
- 矩阵（debug 与 release **逐字节相同**）：sha256
  `e1fe4938650653a58cc34375fe36ff85399ef8856049cb733d01f28d9862b850`；
  `drawn_at_240` **1376**（1351 + 25）；fallback **583**；error 0；
  `mandatory_gaps` 48；`integrity_errors` 空。
- 三数并列：①绝对 source_fallback **583** ②超出认证 profile/冻结权威 **289**
  ③本轮净减少 **25**。

## 5. 评审与效率

- **设计评审（双镜头，RFC 阶段）**：r1 REQUEST-CHANGES → r2 APPROVE（两镜头）。
- **PRR**：r1 REQUEST-CHANGES（E2a 重钉 88 错）→ r2 REQUEST-CHANGES（仍 88）→
  r3 APPROVE（86）→ RFC `implementable`。
- **Amendment 复审**：r4 REQUEST-CHANGES（gate 段仍留 86/23/19 行口径）→ r5
  REQUEST-CHANGES（残余 stale 接线句/表结构）→ **r6 APPROVE**（22 out-of-cohort、
  24 scanned、80/24、25/16 全部逐字复核）。
- **P1+P2 代码评审**：c1 REQUEST-CHANGES（B1 区域游标数组 `owner+1` 越界；
  m1 侧表增长未预付；m2 Python 镜像 ltrim 不一致；m3 `accTitle: ""` 未拦截；
  M1 RFC 数字）→ 全部修复后 **c2 APPROVE**（单条注释算术 NIT 已修）。
- **efficiency retro**：已追加到 `docs/dev/engineering-efficiency.md`，新增 rule 41
  （源形状阻塞器必须在准入它的同一提交内退役）。
- **实测 vs 预估（记录为效率缺陷）**：RFC 的 E2a/E2b/0046 足迹为投影，实测分别
  80 / 25 / 零迁移；已在 RFC 修订与 retro 中记录，避免下次再以“file:line → 新常量”
  当证据。

## 6. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退（本片 25 行亦在 40 列回退、页脚超视口）、
ER107 B3 三门未动。

## 7. 全量 debug ctest

- **run1** `/tmp/loom-logs/ctest-full-dbg-0048.log`（sha256 `5f02dbb8…`）：
  `99% tests passed, 1 tests failed out of 4699`，`CTEST_EXIT=8`。红项
  `Tools.TaskStopCancelsRunningBackgroundAgentDuringBashToolExecution`（5.78 s，与
  Mermaid 无关：本片只改 mermaid relations 解析/布局/发射与账本；该测试在负载下启动
  后台 agent 并取消）。隔离复跑 **3/3 通过**（119/120 ms 级，日志
  `/tmp/loom-logs/iso-taskstop-0048-{1,2,3}.log`）。本轮没有任何 mermaid 测试失败，
  `MermaidOfficialConfig110Native…` 亦通过。
- **run2** `/tmp/loom-logs/ctest-full-dbg-0048-run2.log`（sha256 `375a7601fb697ca7c594e0f774e0d554ed6b0e46c1eb2395f15c4ebe4605a579`）：
  **100% tests passed out of 4699**，`CTEST_EXIT=0`。run1 的红项本轮通过；
  4 项 `E2E_SubagentLive_*` 为既有的可选 live 测试跳过，不计失败。

## 8. 提交形态（与 RFC 的偏差，已披露）

RFC 的 rollback 行原计划“每阶段一个原子提交”。实际交付为**单个本地提交**：
账本、roster 与矩阵产物是 P1/P2 共享的（25 行同一账本、同一矩阵分支、同一 roster
数字），拆成两笔会留下一个无法通过自身门（账本行数与 E2a/E2b 计数不一致）的中间态。
提交体注明 `RFC-0048` 与两个阶段，diff 可按 hunk 回退到任一阶段之前的状态。
TL 未要求 push；本次不 push、不 merge。

## 9. 未验证 / 剩余风险

- 跨区域边、区域内 `direction`、空区域等文法负例只由本片门覆盖；语料中无此类行，
  未来语料扩展需重审。
- `docs/dev/mermaid-unsuccessful-cases.md` / `mermaid-failure-inventory.json` 是
  RFC 0034 时代的检查点快照（未消费、since 0035 未刷新），本片按 RFC 披露而不批量
  重写，留作独立文档任务。

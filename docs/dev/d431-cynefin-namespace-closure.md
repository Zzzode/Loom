# D-431 —— cynefin 封闭 frontmatter 表（5 行，家族收口）

日期 2026-11-23 · 本地，未 push · 承接 D-428 普查与 TL 持续授权。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与判据

- 蒸发候选（body-only census）中 cynefin 的 5 条 fallback 全是配置阻塞，且共享两个封闭形状。
- **预期**：5 行（cynefin 5 → 0，全局 542 → 537，formal 1417 → 1422）。
- **判据（语义，D-412）**：原生投影逐行转写，不画边界曲线/颜色/尺寸，也不画 Mermaid 自动
  生成的领域描述，因此 `config.cynefin.{boundaryAmplitude, seed, width, height,
  showDomainDescriptions}` 与 `themeVariables.cynefin.{complexBg, clearBg, boundaryColor}`
  都是**惰性披露**（页脚 `Cynefin native projection v1`，颜色去引号，
  `boundaryColor` 用边界色措辞）。oracle：画布 = 去 config 孪生画布。

## 2. 落地内容（单文件 `mermaid_cynefin_impl.cpp`）

- 状态机：description 载荷 → `config:` → `cynefin:` / `themeVariables:` → 封闭叶；
  重复叶、未知叶、错缩进、两种嵌套混用一律整源回退；body 风格门不变。

## 3. 门与账本

- 账本 `tools/mermaid/cynefin-ns-d431-cohort.json`（5 行；digest `da252278…`）
  + 生成器 `--check`（Python 镜像）；门 `test_mermaid_cynefin_ns_d431_cohort` 3 项
  （账本钉 / 孪生等式 / 8 条叶正例 + 9 条负例 + body 风格）。
- 矩阵分支置于 RFC 0031 cynefin cohort 之前 + 规则 38 reach proof。

## 4. 既有断言迁移

- `MermaidCynefinCohort.GatedCynefinsStayWholeSource` 的 `frontmatter_gated` 5 行改为
  「必须是 D-431 账本成员」；`VisualGateIsKeyBased` 的 `seed: 42` 负例换成表外叶
  `cynefin.colour`（断言只加不减）。

## 5. 验证（迭代口径，逐用例名）

- 首次红项 `MermaidCynefinCohort.VisualGateIsKeyBased`；迁移后
  `ctest -R 'Cynefin|Radar|Packet|Treemap|Pie|Charts'` = **192/192 通过**，
  `CTEST_EXIT=0`，该用例名与四个 D-431 用例名在日志中逐字 `Passed`
  （`/tmp/loom-logs/ctest-cynefin-d431-run2.log{,.exit}`）。
- 矩阵：1417 → **1422**、fallback **537**、gaps 48、integrity 空、六条 reach proof；
  roster **537 / 36 类**（`family_residual:cynefin` 与 `dagre_layout_tuning` 计数下降），
  self-test / matrix 绿。
- release 目标编译通过；新门 release 下 3/3。
- 三数：①**537** ②**289** ③本轮净减 **5**。

## 6. 未验证 / 剩余风险

- 颜色/几何/描述开关在终端不生效（只披露）；语料外的 themeVariables 键仍整源回退。
- 全量 ctest 与双预设字节门留到主线收口。

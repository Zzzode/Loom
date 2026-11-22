# D-430 —— radar 封闭 frontmatter 表（3 行，家族收口）

日期 2026-11-23 · 本地，未 push · 承接 D-428 普查与 TL「按普查继续下一族」指令。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与判据

- **为什么选它**：D-428 普查推荐第二片；radar 3 条 fallback 全是封闭 frontmatter 形状。
- **预期降幅**：3 行（radar 3 → 0，全局 545 → 542，formal 1414 → 1417）。
- **判据（语义，D-412）**：原生投影逐行转写雷达轮廓，不画多边形/颜色/字号，因此
  `theme: base`、`themeVariables.{fontSize, cScale0, radar.axisColor}`、
  `config.radar.{marginTop, axisScaleFactor}` 全部**惰性披露**（页脚
  `Radar native projection v1`，规范顺序）；颜色页脚输出**去引号归一值**
  （`#FF0000`）。oracle：画布 = 去 config 孪生画布，页脚逐行精确。

## 2. 落地内容（单文件 `mermaid_radar_impl.cpp`）

- 新增 `radar_frontmatter()` 状态机：description 载荷 → `config:` →
  `themeVariables:` / `radar:` → 封闭叶；两级 `radar:` 用**缩进**区分
  （嵌套 axisColor 缩进 6，config 级 marginTop/axisScaleFactor 缩进 4），不可混用。
- 任意其他主题名、未知叶、非法值、错缩进、重复叶 → 整源回退；body 风格门
  （`classDef`/`style`/`:::`/`%%{init}`）不变。
- 页脚每行 ≤96 字节并计 Work。

## 3. 门与账本

- 新账本 `tools/mermaid/radar-ns-d430-cohort.json`（3 行；digest `2f091950…`、
  manifest sha `c966a725…`）+ 生成器 `generate_radar_ns_d430_cohort.py --check`
  （含逐行 Python 镜像）。
- 新门 `tests/ui/test_mermaid_radar_ns_d430_cohort.cpp` 3 项：账本钉；3 行
  「绘制 + native 帧 + 40 列状态 + 页脚精确 + 画布 = 去 config 孪生」；
  六个叶正例（含归一化颜色）与 11 条负例（look、表外主题、未知叶、非法值、
  重复、错缩进、两种 `radar:` 作用域混用）＋body 风格三条。
- 矩阵：`radar_d430_cohort_map()` 分支置于 RFC 0031 radar cohort 之前，规则 38。

## 4. 既有断言迁移（本片足迹）

- **RFC 0031 radar 门**：`frontmatter_gated` 3 行原本断言回退，改为「必须是 D-430
  账本成员」；`body_style_gated`（0 行）不变。

## 5. 验证（迭代口径，逐用例名核验）

- 定向：`MermaidRadarNamespaceD430Cohort.*` 3/3 + generator；
  `MermaidRadarCohort.*` 7/7——首次失败的 `MermaidRadarCohort.GatedRadarsStayWholeSource`
  的通过名称已在本轮 `ctest` 日志中逐字出现。
- `ctest -R 'Radar|Packet|Treemap|Pie|Charts'` = **181/181**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-radar-d430.log{,.exit}`）。
- 矩阵（debug）：`drawn_at_240` **1417**、`source_fallback` **542**、error 0、
  gaps 48、integrity 空、五条 reach proof 通过；roster **542 / 37 类**，
  `--self-test` 与 `--matrix` 绿。
- release 预设：radar 两个目标编译通过，新门在 release 下 3/3。
- 三数并列：①**542** ②**289**（未变）③本轮净减 **3**。

## 6. 未验证 / 剩余风险

- 颜色/字号/布局在终端不生效（只披露）；未覆盖的 themeVariables 键（如
  `cScale1`、`axisLabelColor`）不在语料内，仍整源回退。
- 全量 ctest 与双预设字节门按 TL 口径留到主线收口一次跑。

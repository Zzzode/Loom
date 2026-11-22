# D-429 —— packet 封闭命名空间 + frontmatter title（2 行，家族收口）

日期 2026-11-23 · 本地，未 push · 承接 D-428 普查推荐与 TL「按普查继续下一族」指令。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与判据

- **为什么选它**：D-428 普查把 packet 列为推荐第一片（唯一阻塞是
  `config.packet`），且只有 2 行，是当前最小的干净缝隙。
- **预期降幅**：2 行（packet 2 → 0，全局 547 → 545，formal 1412 → 1414）。
- **判据（语义，D-412）**：packet 的原生投影逐行转写 `位置: "标签"`，**不画位栅格**，
  所以 `showBits`/`bitOrder`/`bitsPerRow` 无法兑现——校验后**惰性披露**（页脚
  `Packet native projection v1` + 每个叶一行，规范顺序 bitOrder/bitsPerRow/showBits）。
  **frontmatter `title:` 则被兑现**：以行内 `title <text>` 形式置于帧首，避免静默丢内容。
  oracle：帧的画布部分等于「去掉 config 块」孪生的画布，且标题文本出现。

## 2. 落地内容（单文件 `mermaid_packet_impl.cpp`）

- 新增 `packet_frontmatter()`：严格状态机（description 载荷 → 可选 title →
  `config:` → `  packet:` → 封闭叶）；未知键、错误缩进、重复、未加引号的 title
  一律整源回退。旧的「出现 config 键就 gate」扫描删除。
- 标题兑现：`title <text>` 置于帧首（与 body 内 `title ...` 语句同形）。
- 页脚：仅在有叶时输出；每行 ≤96 字节并计 Work。
- body 风格门不变（`classDef`/`style`/`:::`/`%%{init}` 仍 gate，带不带命名空间都一样）。

## 3. 门与账本

- 新账本 `tools/mermaid/packet-ns-d429-cohort.json`（2 行；digest `aca34405…`、
  manifest sha `c966a725…`；counts titles 2 / showBits 2 / bitOrder 1 / bitsPerRow 1）
  + 生成器 `generate_packet_ns_d429_cohort.py --check`。
- 新门 `tests/ui/test_mermaid_packet_ns_d429_cohort.cpp` 3 项：账本钉；2 行
  「绘制 + native 帧 + 40 列状态 + 页脚精确 + 标题出现 + **画布 = 去 config 孪生**」；
  封闭文法正例（五个叶值、标题组合）与负例（theme/look、未知叶、`yes`/`sideways`、
  `bitsPerRow` 0/65、重复、错缩进、未加引号/重复 title、body 风格）。
- 矩阵：`packet_d429_cohort_map()` 分支置于 RFC 0031 packet cohort 之前，规则 38。

## 4. 既有断言迁移（本片足迹）

- **RFC 0031 packet 门**：`frontmatter_gated` 两行原本断言回退，改为「两行必须是
  D-429 账本成员」（分片保留，正例由新门拥有）；`body_style_gated`（当前 0 行）不变。

## 5. 验证（迭代口径，逐用例名核验）

- 定向：`MermaidPacketNamespaceD429Cohort.*` 3/3 + generator 4 项；
  `MermaidPacketCohort.*` 7/7——**首次运行红的正是
  `MermaidPacketCohort.GatedPacketsStayWholeSource`，其通过名称已在本轮日志中逐字出现**。
- `ctest -R 'Packet|Treemap|Pie|Charts'` = **169/169**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-packet-d429.log{,.exit}`）。
- 矩阵（debug）：`drawn_at_240` **1414**、`source_fallback` **545**、error 0、
  gaps 48、integrity 空、四条 reach proof 通过；roster **545 / 37 类**。
- release 预设：两个 packet 目标编译通过，新门在 release 下 3/3。
- 三数并列：①**545** ②**289**（未变）③本轮净减 **2**。

## 6. 未验证 / 剩余风险

- `showBits`/`bitOrder`/`bitsPerRow` 在终端不生效（无位栅格），只披露；标题兑现。
- 全量 ctest 与双预设字节门按 TL 口径留到主线收口一次跑。

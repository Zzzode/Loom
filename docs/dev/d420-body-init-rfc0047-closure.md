# D-420 —— RFC 0047 收口：journey 家族 in-body `%%{init}%%` 惰性指令准入（9 行）

日期 2026-11-22 · 本地，未 push · 承接 D-419 选片与 TL 常驻授权。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债。

## 1. 落地内容（P1，一次原子提交）

- **一个闭合文法，两个启用家族。** 三个 TU 的 `journey_fam_visual_config`
  仍是同一份字节实现；新增 `kFamilyInitDirectivesEnabled`（journey/timeline
  true，gantt false）与 `kFamilyInitThemeTable`（4 / 5 / 0）。
  前导指令块的结构化扫描：跳过空行与 `%%` 注释；候选行先剥 `\r`，允许前导
  空格；必须以 `}%%` 结尾；载荷只接受裸 `init`、空 `{}` 或 `logLevel`/`theme`
  扁平对象；引号状态机（未闭合引号、控制字节、单引号内反斜杠均拒绝）；键值经
  `journey_fam_scalar`；嵌套对象、未知键、重复键、超表值、多行/尾随文本全部
  回退。从图头开始的非前导区域保留原“见 `%%{` 且含 theme 即回退”规则。
- **G5 互斥**：frontmatter 视觉叶 + keyed 指令 = 回退；frontmatter 叶 + 裸
  `init` 仍准入。
- **页脚**：header 后按 `config.look` → `config.theme`（frontmatter）→
  `config.logLevel=<v> -> logger only; not applied` →
  `config.theme=<v> -> colour palette; not applied`；行长 45–51 字节。
- **单源边界探针**：`tools/mermaid/journeyfam-init-0047-probes.json`（60 条）
  同时驱动 Python 镜像与 C++ 门；账本以 `selection.probes_sha256` 绑定评审。
- **矩阵分支**置于 clean-only 的 RFC 0031 journey/timeline/gantt 分支之后
  （这些分支对这 9 条 fallback 路径返回 null）、RFC 0040 家族分支与泛化回退之前；
  post-loop 断言 9 行的 `Observation.reason` 命中且该分支不匹配集合外路径
  （rule 38 的“证明到达”）。

## 2. 既有账本/测试同步（本片足迹）

- timeline：`themed_excluded` 7 → 2，新增 `init_admitted` 5；分区
  7/16/2/5（五路，和 30）。
- journey：`themed_excluded` 4 → 0，新增 `init_admitted` 4；10/4/0/4（和 18）。
- gantt：不变（39/11/0/2）；生成器新增 kind 白名单 fail-closed 断言。
- 0040：仅 `selection.exclusions` 文案；admitted 集合与 digest 不变。
- 三个家族测试：timeline 的 `themed_excluded` 循环保留 2 条并配对指令正例与
  `theme: neo` 负例；journey 的空桶循环配对 4 条 `init_admitted` 正例与
  out-of-table/未知键/G5 负例，`with_log_comment` 源新增页脚断言；0040 的
  journey 循环改为对 0047 账本的 4 行正例 + 桶计数断言（不删断言）。
- roster `--emit` 608 / 40 类；`family_residual:journey` 消失，
  `family_residual:timeline` 7 → 2。

## 3. 验证（收口实测）

- 定向：`MermaidJourneyFamInit0047Cohort.*` 5/5；timeline 10/10、journey 6/6、
  0040 7/7。
- `ctest -L mermaid`：**1508/1508 通过**，`EXIT=0`，日志
  `/tmp/loom-logs/ctest-mermaid-0047.log`（4253.21 sec*proc，real 535.27 s）。
- 矩阵（debug）：`original ALL wide_240` **1537 drawn / 608 fallback / 0 error**；
  journey **18/0**、timeline **28/2**、gantt 50/2 不变；
  `drawn_at_240` **1351**；`mandatory_gaps` 48；`integrity_errors` 空。
- roster `--emit` **608 / 40 类**；`--self-test`（608/40/零未分类/可复现）与
  `--matrix`（608 行全匹配）绿。
- 三数并列：①绝对 source_fallback **608** ②超出认证 profile/冻结权威 **289**
  ③本轮净减少 **9**。

## 4. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退（本片 9 行亦在 40 列回退，页脚超视口）、
ER107 B3 三门未动。

## 5. 评审与效率

- **设计评审（双镜头）**：证据/足迹镜头 r1 REQUEST-CHANGES（补 CMake 注册等）
  → r2 APPROVE；文法/架构镜头 r1 REQUEST-CHANGES（gantt 开关、CRLF、引号状态机、
  G5 机制等）→ r2 APPROVE。RFC 状态 `accepted`。
- **PRR**：r1 REQUEST-CHANGES（mirror 保真须有单源探针）→ r2 REQUEST-CHANGES
  （探针仍非单源、schema 不完整）→ r3 APPROVE；随后 RFC `implementable`。
- **P1 代码评审**：r1 REQUEST-CHANGES（2 MAJOR：逐行 oracle 未做 frame=twin+footer、
  probe/cohort digest 仍为 PENDING 使评审绑定失效；4 MINOR + 3 NIT）→ 修复后
  r2 APPROVE（剩余 2 条文档措辞已在本轮修掉）。
- **双预设矩阵逐字节相同**：debug/release 报告 sha256 均为
  `86e4942375205d111af0d39d868ba7cdf299502b0a4f3a9f152900e5ed368ffe`。
- **全量 debug ctest**：首跑 `/tmp/loom-logs/ctest-full-dbg-0047.log`（1142688 B，
  sha256 `a5a523d26bac7d73884aa3ccff79a3d0b5b4d508d701646bc32abdf7593a266e`）为
  `99% / 1 failed out of 4693`、`CTEST_EXIT=8`；红项是既有的临界超时
  `MermaidOfficialConfig110Native.EveryWholeSourceIsObservedBeforeDecodeAndAllProfilesRemainMandatory`
  （预算 300 s；RFC 0046 的 run3 全量中同一条已耗时 240.09 s），隔离复跑 **3/3
  通过**（213.20 s / 249.67 s / 260.00 s，日志 `/tmp/loom-logs/iso-2621-{1,2,3}.log`）。
  无关性证据：该测试的成本由 2145 源 × 110 profile ≈ 236k 次渲染主导，本片改动
  只对其中 journey/timeline/gantt 的 ~100 个源各加一次 O(bytes) 扫描。
- **全量 ctest 复跑**：`/tmp/loom-logs/ctest-full-dbg-0047-run2.log`（1145359 B，
  sha256 `ba20d5938143bd30e259829f229b13f534353a8864bb0c63e65f10bb2787b64e`）为
  `99% / 2 failed out of 4693`、`CTEST_EXIT=8`，两条均为 Timeout：
  `MermaidOfficialConfig110Native…`（300.05 s / 300 s）与
  `MermaidBlock58AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts`
  （58.85 s / 60 s）。后者在同代码的 run1 中以 **51.84 s 通过**（86% 预算），
  隔离复跑 3/3 通过（25.0 / 25.2 / 25.3 s，日志
  `/tmp/loom-logs/iso-3836-{1,2,3}.log`）；它调用
  `tools/mermaid/verify_block58_admission.py --self-test`，本片未触及该脚本或其输入。
  两轮全量的红项互不相同、均在隔离下干净复跑，且本片只改 journey-family 渲染 TU
  与对应账本/测试——负载驱动的临界超时，而非回归。
- **全量 ctest 第三跑**：`/tmp/loom-logs/ctest-full-dbg-0047-run3.log`（1142688 B，
  sha256 `bd89524508a106a4f0bcaf305900862e2aab0547f5f9bfb1996aa95b391e4c83`）为
  `99% / 1 failed out of 4693`、`CTEST_EXIT=8`，红项仍只有
  `MermaidOfficialConfig110Native…`（300.05 s / 300 s）；`MermaidBlock58AdmissionJoin…`
  本轮通过。该测试在三轮全量中均为 300 s 预算的临界运行，而隔离 3/3 通过
  （213.20 / 249.67 / 260.00 s），并在 RFC 0046 的 run3 全量中曾以 240.09 s 通过。
  **结论**：负载驱动的近预算超时；其成本是 110 个 config110 案例的几何复核（约
  2 s/案例），本片对其中 journey-family 源只增加一次 O(bytes) 扫描。已作为
  效率缺陷记入 `docs/dev/engineering-efficiency.md`（需要单独的预算/范围整治，
  不属本片授权范围）。
- **efficiency retro**：已追加到 `docs/dev/engineering-efficiency.md`，新增
  rule 39（选片扫描必须先剥 frontmatter 再结构化分类；折叠 description 可含
  指令样式散文，且 `[^}]*` 正则会漏掉嵌套载荷）与 rule 40（后台构建不可信
  wrapper 退出；只有 builder 进程自身退出或 `pgrep -f '[c]make --build'` 为空
  才证明二进制新鲜，陈旧二进制会静默复现上一个里程碑的矩阵）。

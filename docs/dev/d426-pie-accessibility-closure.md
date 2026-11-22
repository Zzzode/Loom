# D-426 —— pie 可访问性 no-op：家族收口（4 行）

日期 2026-11-23 · 本地，未 push · 承接 D-425 与 TL「按既有授权继续残余家族」指令。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与预期（TL 要求写进测试文档）

- **为什么选它**：D-425 之后 pie 家族只剩 4 条 fallback（dark/default/forest/neutral
  四条 baseline theme 行），且是已定位的小步。**开工前实测修正了 D-425 的判断**：
  4 条里只有 `dark` 是「主题已在表内、仅被 acc 行阻塞」，其余 3 条的主题值
  （`default`/`forest`/`neutral`）本来就不在 RFC 0042 的封闭表内——所以本片是
  两件事：可访问性 no-op + 主题表扩三个基线值。
- **预期降幅**：4 行，pie 家族 4 → **0**（全局 557 → 553，formal `drawn_at_240`
  1402 → 1406）。
- **判据（语义，D-412）**：`accTitle:`/`accDescr:` 是可访问性元数据（Mermaid 只写
  到 a11y DOM），文本终端没有独立可访问性树，因此**校验后消费为 no-op**；
  oracle 是最强形式——**整帧（画布 + 页脚）等于删掉两行 acc 的同源孪生**，逐字节。

## 2. 落地内容（单文件 `mermaid_charts_impl.cpp`）

- **body 文法**：pie 体内标量 `accTitle:`/`accDescr:` 各至多一次；值先 trim，
  成对引号剥离后再测空；非空、≤256 字节、无控制字节。block 形式 `accDescr {`、
  重复、空载荷、超长、控制字节、裸标识符（无冒号）全部整源回退。
- **封闭主题表扩展**：`kPieThemeTable` 5 → 8，加入 `default`/`forest`/`neutral`，
  仍为封闭集合、惰性披露（`config.theme=<v> -> colour palette; not applied`）。
- 未改 RFC 0042 主题 profile 的结构与页脚；未改 D-425 namespace profile。

## 3. 门与账本

- 新账本 `tools/mermaid/pie-acc-d426-cohort.json`（**4 行**，themes 各 1）+ 生成器
  `generate_pie_acc_d426_cohort.py`（`--check`）。钉：cohort digest `d0618087…`、
  manifest sha `c966a725…`、逐行 `twin_sha256`（= 删除 acc 行后的源）。
- 新门 `tests/ui/test_mermaid_pie_acc_d426_cohort.cpp` 3 项：账本钉与孪生摘要；
  4 行「绘制 + native 帧 + 40 列回退 + 页脚精确 + **整帧与 acc-free 孪生逐字节相同**」；
  负例 8 条 + 超长（300 字节）与表外主题 `base`，正例覆盖 8 个封闭主题值与引号载荷。
- 矩阵：`pie_d426_cohort_map()` 分支置于 D-425 与 RFC 0042 之前，规则 38 reach proof。

## 4. 既有断言迁移（本片足迹）

- **RFC 0042 门两处翻正**（只加不减）：
  - `ThemeTableIsClosed` 的负例表原含 `default`/`forest`/`neutral`，本片起它们入表；
    负例换成仍在表外的 `night`/`reduced`/`redux-base`（条目数不变），入表侧由
    D-426 门的正例覆盖。
  - `GrammarBattery` 的「四条 body-blocked 行必须整源回退」翻为「四行必须绘制且
    页脚正确」——这是加强而非放宽，帧等式由新门钉住。
- E2a/E2b 与 Pie7 补充控制均不涉及 pie 主题/acc 形状，实测无迁移。

## 5. 验证（迭代口径）

- 定向：`MermaidPieAccessibilityD426Cohort.*` 3/3；`MermaidPie0042Cohort.*` 6/6；
  `ctest -R 'Pie|Pie7|Charts|Xychart|Layouttuning|Journeyfam0047'` = **166/166**，
  `CTEST_EXIT=0`（`/tmp/loom-logs/ctest-pie-d426-run2.log{,.exit}`）——首次运行
  因 0042 门负例未迁移而红，迁移后全绿。
- 矩阵（debug）：`drawn_at_240` **1406**、`source_fallback` 553、error 0、
  `mandatory_gaps` 48、`integrity_errors` 空、三条 pie reach proof 通过。
- roster `--emit`：**553 / 39 类**，`family_residual:pie` 消失（pie 家族
  **24 originals 全部绘制、0 fallback**）；`--self-test` 与 `--matrix` 绿；
  E6c 冻结缺口 roster 未动。
- release 预设编译（`-Werror`）通过；按 TL 迭代口径，**全量 ctest 与双预设字节比对
  留到主线收口**一次跑（届时给出退出码 + 用例数 + 100% passed）。
- 三数并列：①绝对 source_fallback **553** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **4**。

## 6. 328 口径差异说明（TL 要求标注）

TL 提到的旧 **328** 口径与当前实测不可直接相加：328 是早期（D-3xx 时代、冻结缺口
roster 与 222 exact-bit 尚未成形）的分桶快照；本轮实测按**当前** roster 的 40→39 类
（现 39 类）与 OfficialMatrix `source_fallback` 计数，①口径为 553。二者差异来自
（a）此间 0035–0048 各 RFC 的已验收准入、（b）222 exact-bit + 67 受保护/冻结的
②桶重新划界。按 TL 指示：先按实测记录并标注差异，待 TL 确认后再统一口径。

## 7. 未验证 / 剩余风险

- 可访问性文本本身不参与渲染（这是契约）；未来若终端要暴露 a11y 文本需要新设计。
- 40 列窄视口下这 4 行仍回退（页脚超视口，历史债未动）；pie 家族宽 240 已 0 fallback。
- 本片未跑全量 ctest / 双预设字节门（TL 迭代口径），留待主线收口。

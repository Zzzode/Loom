# D-432 —— c4 `config.wrap`（1 行）

日期 2026-11-23 · 本地，未 push · 承接 TL「按普查表取下一族，一片一提交」指令。
排除项不变：受保护 43、flowchart exact-bit 222、三笔历史债、ER107 冻结面、
10 个受保护文件、0047 及以前的已验收产物。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：D-428 体部 census 里 c4 只剩 1 条「仅配置阻塞」的 fallback
  （`c4/nightly-check/description-wrapping.mmd`），是当前最小且形状最封闭的一片：
  frontmatter 只有折叠 description + `config:` → `  wrap: true`。
- **预期降幅**：1 行（c4 9 → 8，全局 537 → 536，formal `drawn_at_240`
  1422 → 1423）。
- **判据（语义，D-412）**：源自身的状态标记就是 `wrap` 叶；终端投影本来就把
  description 折行显示，SVG 级别的 `text wrapping` 没有额外的终端对应物，因此该叶
  惰性且必须**披露**（`config.wrap=<v> -> SVG text wrapping; not applied`），
  画面文字与 `config` 剥离后的孪生逐行相等。除这一形状外任何键、缩进、重复、
  取值都整源回退。

## 2. 落地内容（单文件 `mermaid_c4_impl.cpp`）

- `c4_frontmatter_wrap()` 是**三态**结果：`accepted=false`（拒绝，整源回退）、
  `accepted=true` 且无叶（普通源 / 剥离 config 的孪生，照常绘制）、
  `accepted=true` + `wrap` 值（绘制并在尾部披露）。三态是必要的：用
  `optional<string>` 表示会让「无 frontmatter」与「未准入 frontmatter」不可区分。
- 状态机：起始 `---` 围栏跳过 → `description:`（仅一次）→ 缩进行是折叠载荷 →
  `config:` → 仅 indent-2 的 `wrap: true|false`（仅一次）；越界键/缩进/重复/取值
  一律拒绝。
- 披露脚注追加在布局尾部：`""` / `C4 native projection v1` / 具体一行；工作在
  预算内（`work.spend`）。
- 体部门不变：`UpdateElementStyle`/`UpdateRelStyle`/`UpdateLayoutConfig` 仍整源回退。

## 3. 门与账本

- 新账本 `tools/mermaid/c4-wrap-d432-cohort.json`（**1 行**）+ 渲染器无关生成器
  `generate_c4_wrap_d432_cohort.py`（`--check`），digest `74d11b9c…`、manifest sha
  `c966a725…`、`wrap: "true"`、`narrow_40: source`（披露行超过 40 列）。
- 新门 `tests/ui/test_mermaid_c4_wrap_d432_cohort.cpp` 3 项：账本钉；
  该行「绘制 + 原样画面 == config 剥离孪生 + 脚注逐字」；封闭叶正例
  （`true`/`false` 各自脚注）与负例（`theme`/`look`/`border`/`wrap: yes`、
  重复 `wrap`、indent-4、顶层 `wrap`、`UpdateRelStyle`/`UpdateLayoutConfig`
  体部门）。
- 矩阵：`c4_wrap_d432_cohort_map()` 分支置于 RFC 0031 c4 cohort 之前，
  规则 38 reach proof（reached set == ledger set）。

## 4. 既有断言迁移（本片足迹）

- **RFC 0031 c4 门** `StyledC4sStayWholeSource`：`styled_excluded` 的 9 行原本全部
  断言回退；改为「D-432 账本成员恰好 1 行（正例由新门拥有），其余 8 行仍整源回退」，
  分片关系保留，未删除任何负例。

## 5. 验证（迭代口径）

- 定向：`ctest --preset debug -R 'C4'` = **12/12**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-c4-d432.log{,.exit}`）；日志中逐字出现刚失败的用例名
  `MermaidC4WrapD432Cohort.LeavesAreInertAgainstConfigFreeTwins` 及
  `ClosedLeafNegativesStayWholeSource`、`LedgerSchemaReviewsAndPins`、
  `GeneratorIsReproducible`、`MermaidC4Cohort.StyledC4sStayWholeSource`，均为
  `Passed`。release 预设同一门 3/3。
- 矩阵（debug，直接跑 `test_mermaid_official`）：64/64 通过，`drawn_at_240`
  **1423**、error 0、`mandatory_gaps` 48、`integrity_errors` 空、reach proof 通过
  （`/tmp/loom-logs/matrix-d432.log`）。
- roster `--emit`：**536 / 36 类**（`family_residual:c4` 9 → 8），
  `--self-test` / `--matrix` 绿；E6c 未动。
- release 预设编译（`-Werror`）通过；全量 ctest 与双预设字节比对留到主线收口。
- 三数并列：①绝对 source_fallback **536** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **1**。

## 6. 未验证 / 剩余风险

- 语料里 `description:` 仅以 `>-` 折叠写法出现；行内标量写法被同一分支接受，但
  没有语料证据，若扩展需重审。
- 40 列窄视口下该行回退（披露行超宽），已由门钉住。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

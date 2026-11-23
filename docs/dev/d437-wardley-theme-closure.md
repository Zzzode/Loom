# D-437 —— wardley 封闭 `config.theme` + column-0 体部（4 行，家族收口）

日期 2026-11-23 · 本地，未 push · 承接 TL「按普查表取下一族，一片一提交」。

## 1. 选片理由与判据（TL 要求写进测试文档）

- **为什么选它**：git 余下三类都需要先裁决（体部 init 披露契约、>8 分支容量、
  共享围栏）；wardley 的 4 条残余是当前唯一「形状封闭 + 语义惰性」的缝隙：
  frontmatter 只有折叠 description + `config:` → `  theme: <v>`。
- **本片发现的真实阻力**：这 4 行**不只是**配置阻塞——它们的体部用上游的
  **column-0** 写法（`wardley-beta` 之后语句不缩进），而渲染器此前只接受两空格
  网格。第一次定向探针因此显示「配置已准入但画面仍回退」，本片把体部文法放宽为
  「column 0 或 2 空格」并保留逐字转写。
- **预期降幅**：4 行（wardley 4 → 0，全局 518 → 514，formal `drawn_at_240`
  1441 → 1445）。
- **判据（语义，D-412）**：终端不画颜色，`theme` 三个词是调色板名，惰性且必须
  披露（`Wardley native projection v1` + `config.theme=<v> -> colour palette; not applied`）；
  判据是「画面（去掉脚注）与去掉 config 的孪生逐行相等」。

## 2. 落地内容（单文件 `mermaid_wardley_impl.cpp`）

- `wardley_frontmatter_theme()` 三态解析（accepted / 无主题 / 有主题），只接受
  折叠 description + 恰好一个 indent-2 `theme:` 叶，值闭集
  base/dark/forest/neutral；顶层 `theme:`、`look:`、重复叶、错缩进一律拒绝。
- 体部网格从「严格 2 空格」放宽为「0 或 2 空格」，其余（缩进 1/3、非法语句、
  样式语句）不变；仍然是逐字转写。
- 尾部追加披露脚注（受行宽/预算守卫）。

## 3. 门与账本

- 账本 `tools/mermaid/wardley-theme-d437-cohort.json`（**4 行**）+ 生成器
  `generate_wardley_theme_d437_cohort.py --check`，digest `4e208f1b…`、
  manifest sha `c966a725…`。
- 新门 `tests/ui/test_mermaid_wardley_theme_d437_cohort.cpp` 3 项：账本钉；
  4 行「绘制 + 脚注逐字 + 去脚注画面 == 去 config 孪生」；正负例（四个主题值、
  表外主题、顶层 `theme`、重复、大小写、带引号、错缩进、`look`/`themeVariables`、
  空 config、体部样式语句，以及 column-0/2 空格接受、1/3 空格拒绝）。
- 矩阵：`wardley_theme_d437_cohort_map()` 分支置于 RFC 0031 wardley 分支之前，
  规则 38 reach proof。

## 4. 既有断言迁移（本片足迹）

- **RFC 0031 wardley 门**：`frontmatter_gated` 4 行改为「必须是 D-437 账本成员」，
  `body_style_gated` 不变；`MalformedInputFailsClosed` 中「indent 0 拒绝」一行按新
  契约改为「indent 0 接受」（indent 1/4 仍拒绝），正例由新门拥有。

## 5. 验证（迭代口径）

- 定向：`ctest --preset debug -R 'Wardley'` = **11/11**，`CTEST_EXIT=0`
  （`/tmp/loom-logs/ctest-wardley-d437.log{,.exit}`）；日志中逐字出现
  `MermaidWardleyThemeD437Cohort.ThemeIsInertAgainstConfigFreeTwins`、
  `ClosedThemeNegativesStayWholeSource`、`GeneratorIsReproducible`、
  `MermaidWardleyCohort.GatedWardleysStayWholeSource`、
  `MermaidWardleyCohort.MalformedInputFailsClosed`，均为 `Passed`。
  release 预设新门 3/3。
- 矩阵（debug，直接跑 `test_mermaid_official`）：**64/64 通过**，`drawn_at_240`
  **1445**、error 0、`mandatory_gaps` 48、`integrity_errors` 空
  （`/tmp/loom-logs/matrix-d437.log`）。
- roster `--emit`：**514 / 35 类**（`family_residual:wardley` 消失），
  `--self-test` / `--matrix` 绿；E6c 未动。
- 三数并列：①绝对 source_fallback **514** ②超出认证 profile/冻结权威 **289**
  （未变）③本轮净减少 **4**。

## 6. 未验证 / 剩余风险

- 体部网格只放宽到「0 或 2 空格」；奇数缩进仍整源回退（无语料证据）。
- 主题表白名单是语料里的四个值；新增主题需要扩表。
- 未跑全量 ctest / 双预设字节门（TL 迭代口径）。

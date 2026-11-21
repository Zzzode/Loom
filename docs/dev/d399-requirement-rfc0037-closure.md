# D-399 —— RFC 0037（requirement 家族）实现收口记录

日期 2026-11-21 · 分支 master · 起点 HEAD `5ffdab17`（RFC implementable）
· 本地执行，未 push、未 merge。RFC 0037 已标记 **implemented**（三阶段全部完成、
全量 ctest 与双预设字节门通过、无受保护文件改动）。
范围：RFC 0037 的独立设计/PRR 评审、P1–P3 三阶段实现、requirement 家族普查收口、
文档与决策登记。

## 1. 家族与选择依据（D-397 §6/§7）

RFC 0036（flowchart）在独立设计评审证伪其测量后被撤回（`0474ec5c`）。随后按同一
探针方法跨家族实测：requirement 148 条 fallback 中 **120 条只被 frontmatter 阻塞**，
body 全部可原生绘制；git 50、state-v2 25、swimlanes 46（但泳道拓扑被压平＝错误几何）。
因此选 requirement（`ac3359da`，D-397 §6），并在 §7 用渲染探针修正为
**120 = 23 P1 + 91 P2 + 6 P3**，排除 24 `layout: elk`、2 命名空间 `config.flowchart.*`、
2 畸形信封。

## 2. RFC 生命周期（agent-run 门）

| 门 | 评审 agent | 结论 |
|---|---|---|
| 设计门 r1 | `agent:a63ae06251ef34362`（独立上下文、自带渲染探针） | REQUEST-CHANGES（2 MAJOR + 4 MINOR + 1 NIT） |
| 设计门 r2 | 同一 agent | APPROVE-TO-ACCEPT |
| PRR prr-r1 | `agent:adaa7ad84dd094605`（可实施性/回滚/防假绿视角） | REQUEST-CHANGES（4 MAJOR + 6 MINOR + 2 NIT） |
| PRR prr-r2 | 同一 agent | REQUEST-CHANGES（2 MAJOR + 3 MINOR + 3 NIT） |
| PRR prr-r3 | 同一 agent | APPROVE-TO-IMPLEMENT |
| P1 代码评审 | `agent:ab5e96a11461da018` | REQUEST-CHANGES（2 MINOR + 4 NIT）→ APPROVE |
| P2 代码评审 | `agent:aa19624ebdb712b6a` | APPROVE（1 MINOR + 3 NIT 均顺手修） |
| P3 代码评审 | `agent:c4f1a7e29b3d60851` | REQUEST-CHANGES（MAJOR：解码器用错）→ APPROVE |

关键设计修正（均来自评审，全部在落地前完成）：
- **设计 r1 MAJOR-1/2**：F0 断言翻转数少算一处（P3 的
  `FrontmatterAndThemeGatesStayClosed`）；census 的 `expected`/`reason` 实际在
  `manifest.json`，且该文件被 `kManifestSha256` 与约 6 个受保护头重复钉死 →
  明确 manifest 不可编辑，改用 Journey 式逐行账本分支。
- **PRR r1 MAJOR-1/3/4**：twin 必须对*全部*已准入叶子取 config-free（否则 99 行
  重复计 footer）；生成器需要 enabled-phase 集合 + 分类优先级 + 每阶段摘要 + review
  归属；构架行必须实测 fan-out（509 个编译任务）而非断言。
- **PRR r2 MAJOR-A/B**：耦合表仍指向 xychart 的硬编码分支（会让窄屏不受门控）；
  标题行不能"twin 帧 + footer"（会重复计 twin 自带 footer）。
- **P3 代码评审 MAJOR**：初稿用 leaf 助手解码标题（接受 `true`/`123`/`a: b`/`[x]`/
  不配对引号），与 §2 钉的 ER 规则不符 → 改为 `er_yaml_value` + `Kind::Text`，
  并补齐七类负例。

## 3. 实现（3 个原子提交）

| 阶段 | 提交 | 内容 |
|---|---|---|
| P1 | `e645212f` | 闭合 `look` 表（classic/neo/handDrawn）+ 披露行；23 行 |
| P2 | `c42647ad` | 闭合 `theme` 调色板表（7 值）+ 披露行（排在 look 之后）；91 行 |
| P3 | `bd542e71` | 顶层 `title:` 经 ER 解码器接受并投到共享 `diagram_title` 缝；6 行 |

每阶段提交都包含：生产代码 + 生成器/账本 + 矩阵分支 + 逐行门禁 +
`fallback_roster.json`（`--emit`，不手改）。生成器按**源码形状**分类全部 172 条
原创（不读 roster），因此阶段晋升不会让钉值自毁；账本采用逐阶段 review 记录，
任一已启用阶段未批准则整体 verdict 保持 `pending`，矩阵分支随即硬失败。

## 4. 普查与字节门

| 指标 | 起点（RFC 0035 收口） | 收口（P3） | Δ |
|---|---|---|---|
| requirement wide_240 drawn | 24 | **144** | +120 |
| requirement wide_240 fallback | 148 | **28** | −120 |
| 全局 wide_240 drawn | 1271 | **1391** | +120 |
| 全局 wide_240 fallback | 874 | **754** | −120 |
| formal `drawn_at_240` | 1085 | **1205** | +120 |
| mandatory_gaps | 48 | **48** | 0 |
| integrity_errors | 0 | **0** | 0 |
| narrow_40 drawn（requirement） | 7 | **7** | 0（无承诺） |

阶段矩阵 sha256（debug 与 release 逐阶段字节一致）：

| 阶段 | 矩阵 sha256 | requirement | 全局 | formal |
|---|---|---|---|---|
| P1 | `36384e6c` | 47/125 | 1294/851 | 1108 |
| P2 | `4a5fbe87` | 138/34 | 1385/760 | 1199 |
| P3 | `7fd2b117` | 144/28 | 1391/754 | 1205 |

Roster 迁移：874 → 851 → 760 → **754**（44 类）。

## 5. 三条债务（未变）

1. xychart rotation 3 行（需要真实几何；OQ-1 裁定）。
2. narrow_40：所有家族仍回退；宽屏准入不是窄屏承诺。
3. ER107 B3 三门（`MATRIX_API_CONFIRMED`、`REVIEWED_HELPER_SHA`/
   `kB3ReviewStatus`、`WRITE_REVIEW_APPROVED`）零改动。

## 6. 验证

- 逐阶段：定向 cohort 门禁（P1 4/4、P2 4/4、P3 6/6）、F0 套件 10/10、生成器
  `--check`、`rfc_lint` 绿。
- 双预设：debug 与 release 全树 `-Werror` 构建通过；每阶段矩阵 sha 字节一致
  （`36384e6c` / `4a5fbe87` / `7fd2b117`）。
- 全量 ctest（第一次，debug）：**4620 项，1 失败**——`MermaidCapabilityReport.
  Requirement11P1P6`（真实冲突，非 flake）：其 P5 畸形前缀负例 5 曾用 `title:` 作为
  "未准入顶层键"的例子，P3 准入标题后该源可以绘制，与"被拒前缀不得绘制 body"的
  契约冲突。改为 `securityLevel: strict`（任何阶段都未准入），契约语义不变；
  sha 钉死的 witness 资产与 `admitted == 11` / `mandatory_remaining == 160`
  聚合零改。此发现已回写 RFC 的耦合表与实现历史（评审期间的清单漏了这一处）。
- 全量 ctest（最终，修复后，debug，`-j$(nproc)`）：
  **100% tests passed out of 4620，CTEST_EXIT=0，269 s**（含 25 个运行期 SKIP 与 2 个
  编译期 DISABLED；本轮未再出现此前记录过的 `ServerMain.DirectConnect*` /
  `E2E_Pty_*` flake）。完整日志 `/tmp/ctest_d399_final.log`。
- 说明：第一次全量运行因 `ctest ... | tail` 丢失退出码与摘要，无法作为证据，故完整重跑；
  该教训已写入 `docs/dev/engineering-efficiency.md` 的 2026-11-21 retro（第 7 条）。

## 7. 未消解 / 推迟项

1. 28 行保持 fallback：24 `layout: elk`（另一套布局引擎）、2 命名空间
   `config.flowchart.htmlLabels`（F0 边界明确关闭命名空间键）、2 畸形信封
   （首行换行、缩进闭合栅栏）——全部显式 non-goal。
2. 无 requirement-corpus 证据的四个上游调色板（`base`、`dark`、`forest`、`neutral`）
   仍拒绝；未来出现语料行才扩展闭合表。
3. 90 条被晋升行没有同 body 的已绘制语料兄弟（上游 visual-check 只为 23 组 body
   提供 `-default`），其"drawn"依赖 D-204 已评审的渲染契约与 twin 门，不是外部对照。
4. 受保护文件、`manifest.json`、`kManifestSha256` 及其重复钉：**零改**；
   无 wire/持久化/settings/session 契约变更。

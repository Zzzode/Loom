# D-410 —— 剩余 669 条 fallback 的根因账单（TL 要求，选家族前先交）

日期 2026-11-22 · 本地，未 push。数据源：当前 `tools/mermaid/fallback_roster.json`
（669 条，44 类）、`/tmp/reach_probe_d402.log`（每行 strip 判定与 frontmatter 键集）、
`tools/mermaid/e6c-mandatory-gap-roster.json`、release 矩阵
`b00ab2c3…`（wide_240 **1476 drawn / 669 fallback**，gaps 48，integrity 0）。

## 1. 口径说明

- 669 条全部是 **wide_240** 的 `source_fallback`；`narrow_40` 是另一个聚合维度，不计入
  这 669 条，因此「40 列谓词失效」不是这 669 条中任何一条的成因（见桶 2）。
- 「可达」判定沿用 D-402：去整块 frontmatter 后 body 在 240 列原生绘制 = strip 可绘
  （204 条），否则是 body 能力债（465 条）。
- 桶按 **处置性质** 划分（TL 五类），一条只进一个桶；数字由脚本现场重算。

## 2. 账单（合计 669）

| 桶 | 数量 | 判据（一句） |
|---|---|---|
| **1. exact-bit / 机器精度** | **222** | flowchart 家族中不在受保护 gap roster 的全部 fallback：该家族以「exact-bit 认证 profile 机器」为唯一准入方式，未认证 profile 无法承诺位级一致（RFC 0036 因此撤回） |
| **2. 视口/布局（终端约束）** | **0** | 669 条全部在 240 列回退；40 列/页脚长度只影响 narrow 聚合，不构成这些行的成因 |
| **3. 缺语料/无外部对照** | **0（作为成因）** | 上游 fixture 全部存在；这一条对 fallback 不成立。它作为**证明强度**问题存在：本轮 pie 4 条 redux 行即无已绘制等价物、只能用合成 twin 自证（RFC 0037/0039 同例），但那是已晋升行的证据等级，不是回退原因 |
| **4. 受保护 roster / 冻结权威** | **67** | `e6c` mandatory gap roster 内的 fallback 43 条（class-diagram 25 + flowchart 18）+ er-diagram 24 条（ER107/ER206 冻结权威 + B3 三门）。改这些要动 protected 列表或破冻结契约 |
| **5. 其余能力债** | **380** | 见下拆分 |
| **合计** | **669** | 222 + 0 + 0 + 67 + 380 |

### 桶 5 的拆分（380，实测重算 2026-11-22）

| 子类 | 数量 | 判据 |
|---|---|---|
| 5a. frontmatter 语义键（strip 可绘、但键非惰性） | **106** | `gitGraph.*` 32、requirement 几何（`layout: elk` 等）25、`pie.*` 8、`treemap.valueFormat` 6、`cynefin.*` 5、`architecture` 4、`state` v2 4、`tree-view` 4、`xychart` 4、`ishikawa.useMaxWidth`/`diagramPadding` 3、`mindmap layout` 3、`radar` 3、`packet` 2、`state` 2、`c4` 1 |
| 5b. 非惰性 `theme/look` 形状 | **15** | 带 `look: handDrawn` / `neo` look / `handDrawnSeed` / `fontFamily` 的行（git 6、agentflow 2、state-v2 2、venn 2、ishikawa 1、requirement 1、state 1）；agentflow 的 2 条已在 RFC 0041 声明为不回退例外 |
| 5c. body 语法/能力/机制 | **253** | 渲染器尚不支持的 body 构造：swimlanes 46、state-v2 45、git 27、usecase 18、sankey 17、mindmap 16、state 11、treemap 10、c4 8、quadrant 7、timeline 7、xychart 7、venn 5、journey 4、katex 4、pie 4、wardley 4、conf-and-directives 3（含需 flowchart 机制的那条 theme 行）、tree-view 3、gantt 2、info 2、requirement 2、zenuml 1 |
| 5d. **惰性 theme-only（立即可做）** | **6** | venn 3（dark、redux-color、redux-dark-color）、ishikawa 2（forest、dark）、railroad 1（dark）——闭合表 + 披露页脚即可，单位成本最低。判据：结构化解析 frontmatter 后语义键集 ⊆ `{config, description, theme}` 且来源不在受保护 roster/flowchart 家族 |

（5a 106 + 5b 15 + 5c 253 + 5d 6 = 380。上一版写的 123/2/252/5 来自 D-402 探针的
`keys=` 字段，该字段把 folded description 的 payload continuation 行也拼进了键集
（如 ``rendered under `config.theme``），既漏判了 venn `9-should-render-with-dark-theme.mmd`，
也把 look/seed 行归错了桶。本版改为直接结构化解析 fixture frontmatter 后重算，
总数 380 不变。这是探针字段的第二次谓词偏差（规则 28 同类），已写入效率规则 36。）

## 3. 对「Mermaid 零 fallback」总目标的影响（TL 要求的事实判断）

按字面（wide_240 零 fallback）**不成立**：要达成需同时
1. 把 flowchart 的 exact-bit 机器扩到 222 条未认证形状的位级一致（家族级重写风险，RFC 0036
   前车之鉴）；
2. 动受保护 `e6c` gap roster 的 43 条（范围变更，需用户拍板）；
3. 解冻 ER107/ER206 权威与 B3 三门（24 条，需另一条 RFC）；
4. 实现 380 条语义/语法能力。

可重述的候选口径（供 TL/用户选择，我不替他们决定）：
- **口径 A**：非 exact-bit、非受保护、非冻结权威家族零 fallback（即桶 1/4 豁免，当前
  剩余 = 380，其中 6 条惰性行本轮即可清零）；
- **口径 B**：「无仅因惰性键回退的行」——本轮之后可为 0（6 条在 RFC 0043 一次清零）；
- **口径 C**：按家族逐个声明「已认证 profile 集合」，fallback 不再作为失败指标（口径变更）。

## 4. 下一步（TL 已指示，不等确认）

先做 **5d：ishikawa + venn + railroad**（6 行 / 3 个机制面，均为既有 key-based frontmatter
gate 的闭合表扩展，RFC 0043 一次并入）。flowchart 的 222 条与受保护 43 条在该口径决定前不动。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

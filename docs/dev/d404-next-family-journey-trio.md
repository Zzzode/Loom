# D-404 —— 下一家族选择：按「可达 drawn 增量 / 成本」实测（journey 三兄弟）

日期 2026-11-22 · 本地，未 push · 承接 Tech Lead 指令（RFC 0039 验收 PASS 已 push，
继续下一家族）与 D-402 的方法。判定口径不变：**可达 drawn 增量 / 每单位成本**。

## 1. 判据

只统计「去掉整块 frontmatter 后 body 能原生绘制、且 frontmatter 里除已建立的闭合表
（`look`/`theme`）之外没有其他阻塞键」的行；再按「新增机制面」计价。上一轮（D-402）
已把 git 切走，剩余排序为：timeline 16、gantt 11、journey 4（三者共用同一个颜色门）、
flowchart ≤11（exact-bit 机器）、agentflow 9、er 5、pie 5。

## 2. 实测（本轮新增，matrix `9d925815` + 形状解析，无新渲染探针）

| 家族 | originals | 现 drawn / fallback | 干净 look/theme 行 | 晋升后 | 剩余 fallback 的成因 |
|---|---|---|---|---|---|
| timeline | 30 | 7 / 23 | **16** | 23 / 7 | 7 条全部经 **body `%%{init}%%` 主题指令**（5 条 per-theme smoke + 2 条 directive 行） |
| gantt | 52 | 39 / 13 | **11** | 50 / 2 | 2 条畸形日期（既有 `fail_closed`） |
| journey | 18 | 10 / 8 | **4** | 14 / 4 | 4 条经 body `%%{init}%%` 主题指令（dark/default/forest/neutral） |

31 行的形状完全一致：top-level 仅 `description` + `config`，description 均为折叠 `>-`。
已绘制行里带**其他 config 键**的实测数量：timeline 0、gantt 3（`gantt.topAxis`/`useMaxWidth`）、
journey 8（`journey.*`）——这 11 行今天能绘制，正是因为既有闸门只拒绝 `theme`/`look`、
其余 config 键一律忽略。任何「严格闭合叶子」的新校验器都会把它们打回 fallback，因此
本切片必须**保持既有 pass-through 语义不变**。

## 3. 为什么是三兄弟一起做

三个渲染器（`mermaid_timeline_impl.cpp` / `mermaid_gantt_impl.cpp` /
`mermaid_journey_impl.cpp`）各有一段逐字相同的「no color contract」闸门，注释明确写着
「the three renderers must not drift apart here」。只改一个家族会立刻制造契约漂移；
因此正确单位是一个 RFC 覆盖三者（31 行），而实现仍按既有惯例**各 TU 本地实现**
（state/requirement/git 三个信封同样是各 TU 本地、以跨家族门禁保证语义一致的），
不引入共享模块/BMI 变更。

对比其他候选：flowchart ≤11（exact-bit 机器，风险最高）、agentflow 9（单文件但
theme 含 base/dark/forest/neutral）、er 5（ER107 冻结权威）、pie 5（无信封）。

## 4. 三笔历史债（本次不动）

1. xychart rotation 3 行。
2. narrow_40 全回退。
3. ER107 B3 三门。

## 5. 复现

- 行集：本轮的判定只用了 `build/release/tests/ui/mermaid-official-matrix.json`
  （`9d925815…`）与 fixture 源码解析，未重新渲染；上一轮 D-402 的渲染探针日志
  `/tmp/reach_probe_d402.log` 仍是 strip-draw 判定的来源（family 行完全一致）。
- 31 行的完整路径与值分布写在 RFC 0040 的 cohort 表，并由新账本的整组摘要钉死。

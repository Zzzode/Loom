# D-411 —— 下一家族选择：D-410 账单的 5d 惰性行（ishikawa + venn + railroad）

日期 2026-11-22 · 本地，未 push · 承接 TL 指令（先交账单、同时做便宜的家族，不停）。

## 1. 判据与样本

D-410 把剩余 669 条 fallback 分桶后，唯一「今天就能清零」的子类是 **5d：惰性
theme-only 行，共 5 条**（ishikawa 2、venn 2、railroad 1）。它们都满足：去 frontmatter
后 body 在 240 列原生绘制（strip=1）、仅被一个 `config.theme` 叶子阻塞、且三个渲染器
各自都有一段「非 theme/look/config 键不拒、但 theme 直接整源回退」的键式闸门。

## 2. 三个家族的结构（实测）

| 家族 | originals | 现 drawn / fallback | 惰性 theme 行 | 其余回退 |
|---|---|---|---|---|
| ishikawa | 13 | 7 / 6 | **2**（forest、dark） | 4 条 `ishikawa.useMaxWidth`/`diagramPadding`/`look: handDrawn` |
| venn | 21 | 11 / 10 | **2**（redux-color、redux-dark-color） | 3 条 look+handDrawnSeed、5 条 body 能力 |
| railroad | 9 | 8 / 1 | **1**（dark） | — |

三族都有既有 cohort 账本（`{ishikawa,venn,railroad}-cohort.json`，字段一致：
`draw_count` / `frontmatter_gated_count` / `frontmatter_gated` / `body_style_gated_count`），
因此同一套四路分区（rows + admitted + frontmatter_gated 剩余 + body_style_gated）可复用。

## 3. 成本与选择

- 机制面：3（每个 TU 本地把 flat 键式闸门改写为 agentflow 式 config 块校验 + 页脚），
  无共享模块/BMI 变更；每族一个闭合表：ishikawa {dark, forest}、venn {redux-color,
  redux-dark-color}、railroad {dark}。
- 为什么不选 flowchart（桶 1 的 222 条）：exact-bit 机器风险 + 18 条还要动受保护 roster。
- 为什么不拆三条 RFC：每族只有 1–2 行，拆开是三次几乎相同的文档/评审成本。

## 4. 目标与副作用（预期）

- ishikawa 7/6 → **9/4**；venn 11/10 → **13/8**；railroad 8/1 → **9/0**；
  全局 1476/669 → **1481/664**；formal 1290 → **1295**；gaps 48、integrity 0 不变。
- roster 669 → **664**；`family_residual:ishikawa` 6→4、`family_residual:venn` 10→8、
  **`family_residual:railroad` 1→0 清空**（RFC 0040 的空类别发射语义保证键集/自检不变）。
- twin：ishikawa 2 行 → `ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd`；
  venn 2 行 → `venn/pr-check/16-...3-set-union...`；railroad 无已绘制等价物 → synthetic。
- narrow_40 一律 live 实测（规则 34/35）。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门。

# D-415 —— 下一家族选择：git `config.gitGraph.parallelCommits`（12 行）

日期 2026-11-22 · 本地，未 push · 承接 TL 常驻授权（RFC 0044 PASS 后直接开下一片，
不等待评审、不换会话；受保护 43 与 flowchart exact-bit 222 不动）。

## 1. 判据

从剩余能力债 351 条（5a 语义键 83 + 5b 形状 15 + 5c body 253）中选片，排序依据与
D-413 相同：①文本惰性可证（渲染器不读该键且有已绘制 twin 或可合成的 config-free twin）；
②机制成本（复用既有准入通道）；③行数/单位成本；④排除改变绘制内容的键、受保护与
exact-bit。

## 2. 候选实测（640 条 roster，结构化解析）

| 候选 | 行数 | 机制面 | twin | 判定 |
|---|---|---|---|---|
| **git `config.gitGraph.parallelCommits`** | **12**（false 2、true 10） | git 既有 `git_frontmatter` 准入通道，新增一个命名空间子树（`gitGraph` + 一个布尔叶） | 12/12 synthetic（无 corpus twin，RFC 0039 同例） | **选中** |
| git 其余 `gitGraph.*` 组合（showBranches / showCommitLabel / rotateCommitLabel / mainBranchOrder / mainBranchName） | 32 | 同一通道但多叶 | — | 保守整体排除：除 1 条全默认值的 no-op 行外，其余改变渲染内容（隐藏分支/标签、改主分支名、改车道顺序） |
| pie `donutHole` + `highlightSlice` | 5 | charts TU 新 profile | 部分 | 次选：行数少、两个叶子 |
| ishikawa `useMaxWidth`/`diagramPadding` | 3 | 命名空间子树 | 待测 | 次选：行数少 |
| state/venn/mindmap/tree-view/xychart/c4 零散 | 2–4 各 | 各族 TU | 部分 | 暂缓：机制面分散 |

## 3. 选中项结构（实测）

- git 家族：originals 135，当前 **70 drawn / 65 fallback**。
- 65 条 fallback 的签名分布：`gitGraph.*` 命名空间 44（12 条仅 parallelCommits、32 条含其他
  内容语义键）、纯 `description:` 14（body 能力债）、纯 look+theme 6（body 密度超限，RFC 0039
  明确例外）、title 1。roster 归属：`family_residual:git` 46、`git_cherry_pick` 12、
  `look_neo` 6、`multiline_label` 1。
- 12 条 `parallelCommits` 行全部落在 `family_residual:git`；frontmatter 形状为
  `description: >-` + `config:` / `gitGraph:` / `parallelCommits: <bool>`；去 config 后
  body 在 240 列原生绘制（probe strip=1）。渲染器从不读取该键（它是上游官方渲染器的
  并行车道合并开关），因此框架 = config-free synthetic twin 帧 + 披露页脚。
- 6 条 `mainBranchName: production` 行与 6 条 `mainBranchOrder: 2` 行虽然也命名空间，
  但会改变上游的主分支名/车道顺序，属于内容语义，**不做**。

## 4. 目标与副作用（预期）

- git 70/65 → **82/53**；全局 1505/640 → **1517/628**；formal `drawn_at_240`
  1319 → **1331**；gaps 48、integrity 0 不变。
- roster 640 → **628**；`family_residual:git` 46 → **34**（类别不消失，43 类不变）。
- 三数报告预测：①**628** ②**289**（未变）③**12**。

## 5. 三笔历史债（未变）

xychart rotation 3 行、narrow_40 全回退、ER107 B3 三门（三门未动）。

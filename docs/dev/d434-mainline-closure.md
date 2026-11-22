# 主线收口记录 —— D-432 / D-433 / D-434 之后（2026-11-23）

本文件是「按普查取下一族」这一轮小片的主线收口证据。全量 ctest 与双预设字节门
按要求只在收口跑一次，日志独立落盘。

## 1. 本轮净变化

| 片 | 提交 | 行数 | fallback | formal `drawn_at_240` |
|---|---|---|---|---|
| 起点（D-431 之后） | `1cd6b180` | — | 537 | 1422 |
| D-432 c4 `config.wrap` | `fe1e3975` | 1 | 536 | 1423 |
| D-433 tree-view 封闭 `config.treeView` | `9ea6aa33` | 2 | 534 | 1425 |
| D-434 tree-view 封闭图标映射 | `73d97178` | 1 | **533** | **1426** |

roster：**533 条 / 36 类**；`--self-test` / `--matrix` 绿；E6c 48 条未动。

三数并列（沿用既有口径）：①绝对 source_fallback **533**；②超出认证
profile/冻结权威 **289**（未变；其构成在 D-413 之前的文档里没有可复核推导，
本文件不重复引用其内部拆解）；③本轮净减少 **4**。

## 2. 全量 ctest（release 预设）

- 命令：`cmake --build --preset release -j8` → `ctest --preset release -j8`
- 构建退出码 **0**（`/tmp/loom-logs/closure-release-build.log{,.exit}`）。
- 测试：**4740 条，100% passed，退出码 0**
  （`/tmp/loom-logs/closure-release-ctest.log{,.exit}`）。
  其中 4 条 `Disabled`、25 条 `Skipped`（既有环境原因），无 `***Failed`。
- `mermaid` 标签合计 1555 条用例、483.78 sec。

## 3. 双预设字节门

- debug `mermaid-official-matrix.json` sha256
  `be4c696f16db7ad18a7a450609a96a97f8e7babe3a3a542c33931ff8d83d26e2`
  （`drawn_at_240` 1426、`mandatory_gaps` 48、`integrity_errors` 空）。
- release 同一矩阵跑完后的 sha256 见 §3.1；两者必须逐字节相同。

### 3.1 release 结果

- release `test_mermaid_official`：**64/64 passed，退出码 0**
  （`/tmp/loom-logs/matrix-release-d434.log`）。
- release `mermaid-official-matrix.json` sha256
  `be4c696f16db7ad18a7a450609a96a97f8e7babe3a3a542c33931ff8d83d26e2`
  （`drawn_at_240` 1426、`mandatory_gaps` 48、`integrity_errors` 空），
  与 debug **逐字节相同**。
- 结论：双预设字节门通过。

## 4. 需要 TL 裁决的岔路（共享改动门槛）

- **现象**：tree-view 最后 1 条与 requirement 的
  `with-a-title-default` 都卡在 `src/ui/visual/mermaid_impl.cpp` 的
  `classify_mermaid`：它只把**恰好**等于 `---` 的行当作 frontmatter 闭合行，
  `---      `（尾随空格）与 `  ---`（缩进）都不算，源进不了任何家族渲染器。
- **实测**：533 条 fallback 中属于该形状的**恰好 2 条**。
- **本地替代**：无——家族解析器已能接受（tree-view 已验证），派发层先拒绝。
- **共享改动**：闭合围栏判定放宽为「`---` + 可选空格/制表符」。
- **影响面**：所有家族派发语义；需重跑矩阵与全量作为验证。
- **另一个候选**：gitGraph `mainBranchName` / `mainBranchOrder`（约 8 行）是
  **要兑现语义**的渲染器特性，建议走 RFC，不走 TL 豁免的小片。

# D-390 — E2E_Pty_Selection_code-down 稳定性治理收口记录

日期 2026-11-20 · 分支 master · 基线 HEAD `f52cd0eb`（D-389 后，ahead 102）· **未 push / 未 merge / 零生产 src 改动 / 测试 harness 层修复**。

## 1. 目标与最小复现

消除全量 4606 中唯一的确定性 PTY 失败 `E2E_Pty_Selection_code-down` (#4565)，不放宽断言、不加 sleep、不 skip、不改生产语义。

- 复现量化（隔离、当前负载）：修复前直接二进制 15 次 = 5 过 / 10 败，失败点 100% 是 `long selection clipboard bytes differ from complete rendered baseline`（剪贴板顶部固定多出 3 行：空行 + `❯ Return the selection fixture` + 空行，expected 2497B vs 实际 2536B，多个失败目录 diff 完全一致）。D-383 还记录过另一种 `unexpected process exit status 0`，本里程碑高负载批次未复现，属同一 PTY 时序族的偶发面。

## 2. 真实机理（strace/帧级证据，区分两种失败面）

通过逐帧解析 `terminal.raw`（NUL 定界）定位到一个**渲染帧时序竞态（测试侧），不是产品 bug**：

- 响应/滚动后帧序列（失败捕获）：`f28 C000@y=2 noecho` → `f29/f30 C000@y=5 echo`（用户消息回显插入，占 3 顶部行，整段代码下移）→ `f31/f32 C000@y=2 noecho`，选择高亮（124 个背景 SGR 组）到 f31 才出现。
- harness 在 `run_selection_case` 滚动循环结束后**立即**用 `decode_frame(latest_frame())` 取锚点，此刻可能落在 f28（C000@y=2）；但鼠标按压被 loom 处理时，回显帧 f29/f30 已把 C000 推到 y=5。loom 的命中测试对**当前已渲染网格**（原子 `screen_` 指针）解析坐标，于是 y=2 的点击落到了 C000 上方 3 行（prompt/空行），剪贴板顶部就固定多出那 3 行。
- 判定性实验：在点击前 drain 2s（仅诊断）后，高负载 6/6 全过，且 drain 后取到的坐标与当前网格一致——证明根因是**批量滚轮事件的重绘在输入返回后仍跨帧 flush**，harness 在重绘未静止时就采样坐标。属于测试时序，产品选择/命中逻辑本身正确（drain 后零失败、字节精确匹配）。
- 失败面关系：剪贴板字节不匹配（本里程碑主因，帧时序）与退出回收竞态（`unexpected process exit status 0`，D-383 偶发）都源于高负载下 PTY 帧/进程调度抖动；本修复让 harness 等渲染静止，消除前者并显著降低整体时序敏感度。

## 3. 修复（harness-only，+33 行，零 src 改动）

`tests/e2e/e2e_pty_shutdown.cpp`：

- 新增 `Terminal::wait_render_idle(quiet_polls=3, cap=5s)`：pump PTY 输出，等到 `output.size()` 连续 3 次 poll 无增长即判定渲染静止。每次等待是对 master fd 的阻塞 `poll`（20ms 仅兜底，非 sleep），应用一静止就返回；5s 只是上限，健康运行几次 poll 即收敛。
- 在 `run_selection_case` 滚动循环结束、取锚点坐标之前调用一次，使点击坐标与 loom 当前命中网格一致。
- **断言强度不变**：精确剪贴板字节比对（`== expected`）、高亮背景、wrap 缩进、退出码等全部保留；未改任何期望值，未 skip，未加固定 sleep。basic 选择路径不经过此调用，行为完全不变。

## 4. 验收证据

- 隔离连跑（直接二进制）：低负载 10/10（load~3.7，~1.55s/次）；两轮受控高负载共 **20/20**（第一轮 load 5→11、第二轮 load 11→14，覆盖原始失败区间下限 14）。合计 **30 连绿**。
- 全 Selection 组 10/10（basic/wide/release-only/streaming/streaming-transcript/wrap-down/wrap-up/wrap-batched/code-down/code-up，load 8–11，`selection-group.log`），共享改动无回归。
- 全量 ctest（单次收口门，load 3.7 启动，`full-ctest/junit.xml`，514s）：**4606 = 4581 通过 + 25 运行期 SKIP + 0 失败 + 2 编译期 DISABLED**。对比 D-389 基线（4580 通过 / 1 失败 / 25 skip / 2 disabled），通过数 +1、**失败 1→0，无净增**。`E2E_Pty_Selection_*` 全 10 例在全量中均 PASS（含目标 code-down）；这是该长周期测试治理链上首个全量零失败结果。残余为 25 个运行期 skip（环境前置不满足，非失败）与 2 个编译期 disabled，均为既有项。

### 关于高负载上限

本机 56 核，可控 CPU 压测最多把 load1 推到 ~14（其余被其他用户的 BigShark/java 与系统占用）；原始 20–28 的极端负载窗口本里程碑不可复现。但本修复是**事件驱动的静止等待**而非时限调大，对负载单调鲁棒（负载越高只是多等几次 poll，静止判定不变），且已在原始失败下限 14 下 20/20。

## 5. 边界

本地提交、不 push、不 merge；十保护文件零改；无生产/持久化/wire/settings/session 契约变更；`git diff` 仅 `tests/e2e/e2e_pty_shutdown.cpp` 一个测试文件。

# D-388 — E2E_Pty_ProgramStatus_question 确定性失败修复收口记录

日期 2026-11-20 · 分支 master · 基线 HEAD `8bb7fe6b`（D-384 之后，ahead 98）· **未 push / 未 merge / 零生产 src 改动 / 测试 harness 层修复**。

## 1. 目标与最小复现

消除全量 4606 中残留的确定性失败 `E2E_Pty_ProgramStatus_question` (#4589)，且不降低断言强度。

- 复现命令：`ctest -R '^E2E_Pty_ProgramStatus_question$' --repeat until-fail:5 -j1`（证据 `/tmp/d388-pty-programstatus/repro-untilfail.log`、`repro-junit.xml`）。
- 关键实测：**在主机低负载 load1=6.46 下仍 10.32s 确定性失败**，失败信息 `whole operation did not finish with the expected terminal status`。这排除了“高负载墙钟超时”假设——等待再久也无法满足谓词。

## 2. 根因（证据化，非超时）

诊断捕获 `/tmp/loom_pty_recovery_PDz0dt/terminal.raw` 解析 OSC7501 实际状态向量：

- 实际（8）：`idle, working, blocked:permission, working, blocked:permission, working, blocked:question, done`
- 期望（9）：上述 + blocked:question 后多一个 `working`
- 确定性前缀 7 项（止于 blocked:question）**逐元素精确匹配**；`PTY status complete` 视觉已出现（操作真实完成）。

与 D-384 同族的**电平采样合并**：生产 `ProgramStatusReporter` 仅在 Start/Flush/Finish 边界发状态且对相同报告去重（`program_status.cppm:10-11`、`program_status.cpp:202`）。question 解答后第二轮模型亚秒级（"Worked for 0s"）从 question 经 working 直达 done，瞬态 working 未撞上渲染 Flush 即被合并。**生产行为正确，是测试要求一个契约不保证的独立帧。** 用户点名的 4 处固定墙钟循环（323/394/589/1042）经核对均服务 mermaid/task/chain/SIGKILL 路径，不在本场景；`wait()` 本就是 20ms poll master fd 的事件驱动收敛，墙钟仅兜底，故无需改等待机制。

## 3. 修复（harness-only，单文件 +44/−3）

`tests/e2e/e2e_pty_shutdown.cpp` 的 `run_status_case`，仅对 `osc7501-question` 一个场景生效（`question` 布尔门控），其余 13 个 ProgramStatus 场景保持原严格向量相等，零行为变化：

- 一次性从 `expected` 推导 blocked:question 前缀（7 项），复用 D-384 已接受的契约助手 `question_recovery_reported`（精确前缀 + 仅允许前缀后瞬态 working 零或多个 + 强制 done 里程碑；任何 blocked/idle/error 非法穿插或缺 done 即红）。
- 最终恢复等待：question 走契约助手，其余场景仍 `status_reports == expected`。
- 沉降后快照真实（合并后）向量；后续合成重绘/畸形 OSC/鼠标输入后的两处 `expect_statuses` 对 question 改为“向量与沉降快照逐元素相等（任何重复/丢失报告即红）+ 契约仍满足”，**完整保留原去重/保留不变量的强度**，不放宽。
- OSC 字段/状态/kind 合法性仍由 `status_reports` 内部 `require` 硬失败；tool_result 精确载荷、prompt 无污染、capability probe 仅一次、退出码 0 等检查全部未动。

## 4. 验收证据

- (a) 目标隔离 3/3 绿：`ctest-run{1,2,3}.xml`（JUnit failure=False，0.57s 级，load1≈1.3）；直接二进制连跑 5/5 PASS（`direct5-run*.log`）。
- (b) `E2E_Pty_ProgramStatus_*` 全组 **14/14 全绿**（`group-results.log`，load1 1.3–1.6，串行带间隔），无回退。
- (c) 单次全量 ctest（低负载 load1=1.28 启动，`full-ctest/junit.xml`）：**4606 注册 = 4580 通过 + 25 运行期 SKIP + 1 失败 + 2 编译期 DISABLED**。对比 D-384 收口（4579 通过 / 2 失败），通过数 +1、失败 2→1，**失败集合未新增反而缩减**。本里程碑四个受关注用例在全量 JUnit 中均 PASS（ProgramStatus_question 0.58s、Question_layout 1.76s、Question_empty 1.27s、BridgeDaemon 12.08s）。
  - 唯一残余失败 `WireTransport...Deadline` (#4498)：隔离 3 次实测 **PASS/FAIL/PASS**（`wire4498-run{1,2,3}.log`，load1≈3），非确定 flake 坐实——真实本地 HTTP + 1500ms 慢头/40×100ms 分块睡眠 + 总 deadline 在调度抖动下越界，与 D-384 判定一致，且本里程碑未改任何 wire/query 文件。判 flake，不改断言，建议纳入时序 flake 治理/低负载窗口复跑。
- (d) diff 仅 `tests/e2e/e2e_pty_shutdown.cpp` 一个测试文件（+44/−3），无 `src/` 改动。

## 5. 边界

本地提交、不 push、不 merge；十保护文件零改；无生产/持久化/wire/settings/session 契约变更。

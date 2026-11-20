# D-389 — WireTransport #4498 时序 flake 治理收口记录

日期 2026-11-20 · 分支 master · 基线 HEAD `7f17b45e`（D-388 后，记录入库 `cce93397`）· **未 push / 未 merge / 零生产 src 改动 / 测试夹具层修复**。

## 1. 目标与最小复现

消除全量 4606 中唯一残余失败 `WireTransport.MetadataSlowHeadersAndContinuousBodyRespectRequestDeadlineAndStillPost` (#4498)，不放宽断言、不加 sleep、不跳过、不改生产语义。

- 复现：`ctest -R '^WireTransport…DeadlineAndStillPost$' --repeat until-fail:8`（`repro-untilfail.log`）首次即红，gtest 失败点 `test_wire_transport.cpp:2717`：`server.metadata_requests().size()` 实测 **2** 期望 1；`SCOPED_TRACE` 在 slow_headers=true/false 两个子用例都出现过，说明机制共通、与具体子用例无关。

## 2. 真实机理（实证，推翻“调度抖动/超时”初判）

四要素（本地 HTTP + 1500ms 慢头 + 40×100ms 分块 + 总 deadline）的耦合点经定位**不是失败原因**：
- strace（`strace-1.log`）按 fd 关联 connect 端口与每个 GET：两个测试迭代各一个 server，客户端对每个 server 恰好发 1 个 `GET /v1/models`（共 2 个 models GET）+ 1 个 POST，连接数与报文数完全正常，无重试、无重发。
- 临时 env 门控诊断（已还原）抓到额外请求真身：`[WIREDIAG] GET path=/ ua=persona-ports`。即开发机 `~/.persona-devbox-cn` 的 **persona 端口扫描 agent** 在测试 ~3.7s 窗口内扫到随机临时端口，发来根级 `GET /`。
- 测试夹具 `WireHttpServer` 注册了全捕获 `server_.Get(R"(/.*)", …)`，把**所有** GET（含根探测）无条件 push 进 `metadata_requests_` 与 `methods_`，于是精确计数 1→2 失败。与 D-384 BridgeDaemon（commit `10bf4574`）是**同一宿主扫描污染、同一类计数竞态**，不是墙钟/deadline 问题——操作本身正常完成，只是台账被外来流量污染。
- 不可压缩墙钟（1500ms/40×100ms 是夹具有意构造的慢响应）与 1s read/2s 总 deadline 均工作正常；因此无需事件化改造或时钟注入，问题在测试夹具的入账边界。

## 3. 修复（harness-only，单文件 +13）

`tests/query/test_wire_transport.cpp` 的 `WireHttpServer` GET 处理器：仅当请求路径以 `/models` 结尾才计入 `metadata_requests_`/`methods_` 并交给 metadata 回调；非 models 的 GET（根探测）直接回 404 且不入账。

- 为何用 `/models` 后缀而非命名空间前缀：引擎把 messages 端点尾部 `messages` 替换为 `models` 得到 metadata 路径，故所有真实 metadata GET 都以 `/models` 结尾，**包括租户前缀 base URL**（`/tenant/acme/v1/models`、`/other-tenant/v1/models`，见用例 2535/2612）。初版用 `/v1/` 前缀曾使两个租户路径用例回归，已纠正。
- 断言强度不变：仍精确要求 metadata 请求数与 `methods()` 向量；根探测本就不是 SUT 流量，过滤它不弱化任何语义。POST 处理器不动（`/gateway/prepared-request` 等合法非 /v1 POST 仍正常入账）。无 sleep、无 skip、无生产改动。

## 4. 验收证据

- 活体过滤实证：带临时打点的运行捕获到真实 `GET / ua=persona-ports` 被 `REJECTED`（404 不入账）同时测试 PASSED（`reject-run-2.log`，打点已还原）。
- 隔离连跑：两批各 5 次共 **10/10 通过**（`high5-final.log` load1≈21、`batch2-5rounds.log` load1≈20），每次 ~3.7s；另 12 次 strace 运行全过、每例恰 2 个 models GET。
- 整个 `test_wire_transport` 二进制 **49/49 全绿**（`wire-binary-full2.log`），含此前回归的两个租户路径用例。
- 全量 ctest（单次收口门，load1≈19 启动，`full-ctest/junit.xml`）：**4606 = 4580 通过 + 25 运行期 SKIP + 1 失败 + 2 编译期 DISABLED**。#4498 WireTransport 在全量中 PASS（3.53s，修复生效）。失败集合与 D-388 基线（同为 4580/25/1/2、1 失败）**总数一致、身份互换**：唯一失败变为 `E2E_Pty_Selection_code-down` (#4565)。
  - #4565 是 D-383 即登记在册的高负载 PTY 选择 flake，与本里程碑零代码交集（本里程碑只改 `tests/query/test_wire_transport.cpp`，该用例在 `tests/e2e/e2e_pty_shutdown.cpp`）。隔离实测非确定：直接二进制 10 次 8 过 2 败（load1≈18），失败信息为 `long selection clipboard bytes differ from complete rendered baseline`（渲染/剪贴板竞态）；ctest 下另见过 `unexpected process exit status 0`（退出回收竞态）。属既有 PTY 时序/渲染竞态族，非本里程碑回归、未新增失败，建议另立 PTY 选择稳定性任务，不在本里程碑扩大范围。
  - 结论：本里程碑**消除了目标 #4498，失败集合未净增**（#4498 出，#4565 作为既有环境 flake 入，二者不同子系统、不同根因）。

### 关于“低负载轮”

本机被其他用户的 BigShark（~13 核）+ java 作业占满，里程碑期间 load1 持续 16–21，真正空闲窗口不可得（非本会话可控）。但本修复是**确定性路径后缀过滤，零 timeout/clock/sleep 依赖**，负载对它不是有效变量；扫描碰撞在高负载下更易发生，故 load~21 下 10/10 是更严格条件。前置缺陷已由 persona-ports UA 实锤为计数污染而非超时，低负载复跑不产生新增信息。

## 5. 边界

本地提交、不 push、不 merge；十保护文件零改；无生产/持久化/wire/settings/session 契约变更。

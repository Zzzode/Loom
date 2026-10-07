/// @file feature_flags.cppm
/// @brief Compile-time feature flags controlling which tools are registered.
///
/// These constexpr booleans control which tools are registered in the runtime
/// tool registry, producing equivalent tool sets at compile time without a
/// bundler.
///
/// To enable a feature for a custom build, flip the corresponding `constexpr bool`
/// to `true` (or pass a `-D` define via CMake if we add that plumbing later).
module;

export module loom.tools.feature_flags;

// Enables the SleepTool.
constexpr bool FEATURE_PROACTIVE = false;
constexpr bool FEATURE_KAIROS = false;

// CronCreateTool / CronDeleteTool / CronListTool registration.
constexpr bool FEATURE_AGENT_TRIGGERS = false;

constexpr bool FEATURE_AGENT_TRIGGERS_REMOTE = false;

constexpr bool FEATURE_MONITOR_TOOL = false;

constexpr bool FEATURE_SEND_USER_FILE_TOOL = FEATURE_KAIROS;

constexpr bool FEATURE_KAIROS_PUSH_NOTIFICATION = false;
constexpr bool FEATURE_PUSH_NOTIFICATION_TOOL =
    FEATURE_KAIROS || FEATURE_KAIROS_PUSH_NOTIFICATION;

constexpr bool FEATURE_KAIROS_GITHUB_WEBHOOKS = false;

constexpr bool FEATURE_VERIFY_PLAN_EXECUTION = false;

constexpr bool FEATURE_OVERFLOW_TEST_TOOL = false;

constexpr bool FEATURE_CONTEXT_COLLAPSE = false;

constexpr bool FEATURE_TERMINAL_PANEL = false;

// Enabled in CPP builds — the browser tool has a working implementation.
constexpr bool FEATURE_WEB_BROWSER_TOOL = true;

constexpr bool FEATURE_HISTORY_SNIP = false;

constexpr bool FEATURE_UDS_INBOX = false;

// Enabled in CPP — workflow tool has a working implementation.
constexpr bool FEATURE_WORKFLOW_SCRIPTS = true;

// In CPP we register it unconditionally; execution is a no-op on non-Windows.
constexpr bool FEATURE_POWERSHELL_TOOL = true;

// Bash is always enabled in CPP builds.
constexpr bool FEATURE_BASH_TOOL_ENABLED = true;

// When true, Glob/Grep tools are suppressed (ant-native shell aliases handle it).
constexpr bool FEATURE_EMBEDDED_SEARCH_TOOLS = false;

// Enabled in CPP — task tools are fully implemented.
constexpr bool FEATURE_TODO_V2 = true;

// Enabled in CPP — LSP tool has a working implementation.
constexpr bool FEATURE_ENABLE_LSP_TOOL = true;

// Enabled in CPP — worktree tools are implemented.
constexpr bool FEATURE_WORKTREE_MODE = true;

// Enabled in CPP — team tools are fully implemented.
constexpr bool FEATURE_AGENT_SWARMS_ENABLED = true;

// In CPP we register the testing tool for all builds.
constexpr bool FEATURE_TESTING_PERMISSION_TOOL = true;

// Enabled in CPP.
constexpr bool FEATURE_TOOL_SEARCH = true;

// Enabled in CPP — script tool has a sandboxed implementation.
constexpr bool FEATURE_SCRIPT_TOOL_ENABLED = true;

export namespace loom::tools::features {

// Re-export the flags under a readable namespace for call sites.

/// SleepTool registration.
inline constexpr bool kEnableSleepTool = FEATURE_PROACTIVE || FEATURE_KAIROS;

/// Cron tools (CronCreate/Delete/List).
inline constexpr bool kAgentTriggers = FEATURE_AGENT_TRIGGERS;

/// RemoteTriggerTool.
inline constexpr bool kAgentTriggersRemote = FEATURE_AGENT_TRIGGERS_REMOTE;

/// MonitorTool.
inline constexpr bool kMonitorTool = FEATURE_MONITOR_TOOL;

/// SendUserFileTool.
inline constexpr bool kSendUserFileTool = FEATURE_SEND_USER_FILE_TOOL;

/// PushNotificationTool.
inline constexpr bool kPushNotificationTool = FEATURE_PUSH_NOTIFICATION_TOOL;

/// SubscribePRTool.
inline constexpr bool kSubscribePRTool = FEATURE_KAIROS_GITHUB_WEBHOOKS;

/// VerifyPlanExecutionTool.
inline constexpr bool kVerifyPlanExecution = FEATURE_VERIFY_PLAN_EXECUTION;

/// OverflowTestTool.
inline constexpr bool kOverflowTestTool = FEATURE_OVERFLOW_TEST_TOOL;

/// CtxInspectTool.
inline constexpr bool kContextCollapse = FEATURE_CONTEXT_COLLAPSE;

/// TerminalCaptureTool.
inline constexpr bool kTerminalPanel = FEATURE_TERMINAL_PANEL;

/// WebBrowserTool.
inline constexpr bool kWebBrowserTool = FEATURE_WEB_BROWSER_TOOL;

/// SnipTool.
inline constexpr bool kHistorySnip = FEATURE_HISTORY_SNIP;

/// ListPeersTool.
inline constexpr bool kUdsInbox = FEATURE_UDS_INBOX;

/// WorkflowTool.
inline constexpr bool kWorkflowScripts = FEATURE_WORKFLOW_SCRIPTS;

/// PowerShellTool.
inline constexpr bool kPowerShellTool = FEATURE_POWERSHELL_TOOL;

/// BashTool enabled.
inline constexpr bool kBashToolEnabled = FEATURE_BASH_TOOL_ENABLED;

/// Embedded search tools (ant-native bfs/ugrep).
inline constexpr bool kEmbeddedSearchTools = FEATURE_EMBEDDED_SEARCH_TOOLS;

/// Todo V2 (TaskCreate/Get/Update/List).
inline constexpr bool kTodoV2 = FEATURE_TODO_V2;

/// LSP tool.
inline constexpr bool kEnableLspTool = FEATURE_ENABLE_LSP_TOOL;

/// Worktree mode (Enter/ExitWorktree).
inline constexpr bool kWorktreeMode = FEATURE_WORKTREE_MODE;

/// Agent swarms (TeamCreate/Delete).
inline constexpr bool kAgentSwarmsEnabled = FEATURE_AGENT_SWARMS_ENABLED;

/// TestingPermissionTool.
inline constexpr bool kTestingPermissionTool = FEATURE_TESTING_PERMISSION_TOOL;

/// ToolSearchTool.
inline constexpr bool kToolSearch = FEATURE_TOOL_SEARCH;

/// ScriptTool.
inline constexpr bool kScriptToolEnabled = FEATURE_SCRIPT_TOOL_ENABLED;

} // namespace loom::tools::features

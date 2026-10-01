// RFC-0001 B15: pre-main global installation of the orchestration runtime
// backends for the test_tools binary. Production calls
// loom::orchestration::install_runtime_backends() once at the top of main()
// (and again, call_once-noop, per server session); gtest_main gives tests no
// such hook, so this namespace-scope initializer performs the same one-shot
// slot assignment before any TEST body runs. This is what makes the lifted
// 'lsp' / 'mcp*' / 'computer_use' dispatch branches and the 'Agent' tool
// resolve through cc.tools.runtime_backends.port exactly as they do in loom
// — no per-test factory bind sites. (The missing-tool MCP fallback was
// never a slot: roots bind it per ToolRegistry; same for the MCP snapshot
// providers.) The image-codec/skill slots are still additionally
// reset by FileToolServicesGuard around the cases that need them.
import std;
import loom.orchestration.runtime_backends;

namespace {

struct RuntimeBackendsPreMainInstall {
    RuntimeBackendsPreMainInstall() {
        loom::orchestration::install_runtime_backends();
    }
};

const RuntimeBackendsPreMainInstall g_runtime_backends_pre_main_install;

} // namespace

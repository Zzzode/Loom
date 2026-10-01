#!/usr/bin/env python3
"""Cumulative Phase B replay: starts from the LIVE module graph parsed by
graph_check.load_units(), applies each execution batch as explicit module
graph edits, and after every batch runs:
  1. Tarjan on the full module graph (must stay a DAG);
  2. non-contract upward-edge diff vs the frozen live baseline (no NEW);
  3. Tarjan over the 9 TARGET_AREAS (CORE8 + loom.orchestration).
Also builds the modeled CMake target link graph before/after and checks
for library-level link cycles.
"""
import copy, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "tools", "arch"))
import graph_check as gc

ALLOW = gc.load_allowlist()
LIVE_BASELINE = gc.load_baseline()  # 13 frozen illegal upward pairs

units = gc.load_units()
LIVE = gc.module_deps(units)
g = {m: set(s) for m, s in LIVE.items()}

def add(mod, imp): g.setdefault(mod, set()).add(imp)
def drop(mod, imp):
    if mod in g: g[mod].discard(imp)
def newmod(mod, imps=()):
    g[mod] = set(imps)
def kill(mod):
    g.pop(mod, None)
    for m in g: g[m].discard(mod)
def rename(old, new):
    """Rename a module; rewrite every surviving importer; preserve edges."""
    imps = g.pop(old, set())
    g[new] = imps
    for m in g:
        if old in g[m]:
            g[m].discard(old); g[m].add(new)

def area_sccs9():
    tg = {a: set() for a in gc.TARGET_AREAS}
    for m, imps in g.items():
        a = gc.area_of(m)
        if a not in tg: continue
        for i in imps:
            b = gc.area_of(i)
            if b in tg and b != a: tg[a].add(b)
    sccs = [sorted(c) for c in gc.tarjan_scc(tg) if len(c) > 1]
    return tg, sccs

def gate_state(tag):
    cyc = [c for c in gc.tarjan_scc(g) if len(c) > 1]
    up = []
    for m in sorted(g):
        ra = gc.rank_of(m)
        if ra is None: continue
        for i in sorted(g[m]):
            rb = gc.rank_of(i)
            if rb is not None and rb > ra and not gc.is_contract(i, ALLOW):
                up.append((m, i))
    new_up = sorted(set(up) - LIVE_BASELINE)
    tg, sccs = area_sccs9()
    print(f"[{tag}] modcycles={len(cyc)} newUp={len(new_up)} "
          f"illegalUp={len(up)} targetSCCs={sccs if sccs else 'none'} "
          f"pass9={not sccs}")
    if cyc: print("   !! MODULE CYCLE", cyc[:2])
    if new_up: print("   !! NEW UPWARD", new_up)
    return not cyc and not new_up and not sccs

print("=== LIVE ===")
gate_state("live")

# ---- B1: F9 delete four zero-importer dead modules + dead services helper
for m in ["loom.commands.mcp.add_command", "loom.cli.handlers.mcp_handler",
          "loom.entrypoints.mcp_entrypoint",
          "loom.ui.features.mcp.mcp_settings_panel"]:
    kill(m)
gate_state("B1 F9-dead-modules")

# ---- B2: F9 loom.config.mcp_types leaf + config re-export
newmod("loom.config.mcp_types")
add("loom.config.config", "loom.config.mcp_types")
gate_state("B2 F9-config-leaf")

# ---- B3: F9 services alias via `export import` + field adaptations
add("loom.services.mcp.types", "loom.config.mcp_types")
gate_state("B3 F9-services-alias")

# ---- B4: F9 loader seam cuts mcp -> loom.config.config
drop("loom.tools.mcp", "loom.config.config")
add("loom.tools.mcp", "loom.config.mcp_types")
newmod("loom.commands.mcp.core_settings_loader",
       {"loom.config.config", "loom.tools.mcp"})
gate_state("B4 F9-loader-seam")

# ---- B5: F4 loom.types.tool_types atomic cut (8 created / 3 deleted)
newmod("loom.types.tool_types")
for m in ["loom.services.streaming_executor", "loom.query.query_engine",
          "loom.tools.agent.utils", "loom.tools.mcp",
          "loom.tools.runtime_message_delivery", "loom.tools.runtime_registry",
          "loom.tools.spawn_multi_agent", "loom.tools.tool"]:
    add(m, "loom.types.tool_types")
for m in ["loom.services.streaming_executor", "loom.tools.mcp",
          "loom.tools.spawn_multi_agent"]:
    drop(m, "loom.tools.tool")
gate_state("B5 F4-tool-types")

# ---- B6: F3/F8 additive snapshot sink in mcp_tool (no edge change)
gate_state("B6 F38-sink-additive")

# ---- B7: interim bridge in loom.bootstrap (outside the 9 target areas)
newmod("loom.bootstrap.mcp_connectivity", {
    "loom.hooks.remaining_notifs", "loom.services.mcp.types",
    "loom.services.mcp.connection_manager", "loom.tools.mcp"})
gate_state("B7 F38-bridge-add")

# ---- B8: atomic cut of hooks<->tools MCP legs
kill_edges = [("loom.hooks.remaining_notifs", "loom.services.mcp.types"),
              ("loom.hooks.remaining_notifs", "loom.services.mcp.connection_manager"),
              ("loom.tools.mcp", "loom.hooks.remaining_notifs")]
for a, b in kill_edges: drop(a, b)
gate_state("B8 F38-atomic-cut")

# ---- B9: CMake link hygiene (no module-graph effect)
gate_state("B9 link-hygiene")

# ---- B10: F10-A loom.skills.file_access.port + agent_resume dead import
newmod("loom.skills.file_access.port")
for m in ["loom.tools.file_read", "loom.tools.file_edit", "loom.tools.file_write"]:
    drop(m, "loom.skills.skill"); add(m, "loom.skills.file_access.port")
add("loom.skills.skill", "loom.skills.file_access.port")
drop("loom.tools.agent.resume", "loom.skills.skill")  # textually dead, deleted
gate_state("B10 F10-file-port")

# ---- B11: F10-B image codec port + loom_orchestration target appears
newmod("loom.tools.image_codec.port")
drop("loom.tools.file_read", "loom.services.image")
add("loom.tools.file_read", "loom.tools.image_codec.port")
drop("loom.tools.runtime_registry", "loom.services.image")  # computer_use TU (will move B15)
add("loom.tools.runtime_registry", "loom.tools.image_codec.port")
newmod("loom.orchestration.runtime_backends", {
    "loom.services.image", "loom.tools.image_codec.port"})
gate_state("B11 F10-codec-orch-born")

# ---- B12: F11-C skill loader executor seam
drop("loom.tools.runtime_registry", "loom.skills.skill")
newmod("loom.tools.runtime_backends.port")  # registry seam leaf (std + tool_types)
add("loom.tools.runtime_registry", "loom.tools.runtime_backends.port")
for i in ["loom.skills.skill", "loom.tools.agent_runtime",
          "loom.tools.runtime_registry", "loom.tools.tool", "loom.utils.json",
          "loom.tools.image_codec.port"]:
    add("loom.orchestration.runtime_backends", i)
gate_state("B12 F11-skill-seam")

# ---- B13: F14 Alpha1 — agent permission types sink
for m in ["loom.tools.runtime_registry", "loom.tools.team_create",
          "loom.tools.team_delete"]:
    drop(m, "loom.tools.agent"); add(m, "loom.tools.agent_types")
add("loom.tools.agent.utils", "loom.tools.agent_types")
gate_state("B13 F14a-agent-types")

# ---- B14: F14 Alpha2/3 — agent_worktree leaf, runtime_team_shared rewire
newmod("loom.tools.agent_worktree", {
    "loom.tools.agent_runtime", "loom.tools.runtime_shared_utils", "loom.utils.git"})
drop("loom.tools.runtime_team_shared", "loom.tools.agent")
add("loom.tools.runtime_team_shared", "loom.tools.agent_worktree")
add("loom.tools.agent.utils", "loom.tools.agent_worktree")  # facade re-export alias
gate_state("B14 F14a-worktree")

# ---- B15: B11 ATOMIC flip (F12+F13+F14-Beta + bridge rehome) ----
# (a) computer_use impl TU leaves loom.tools.runtime_registry for orch
drop("loom.tools.runtime_registry", "loom.tools.image_codec.port")
for m in ["loom.tools.mcp", "loom.tools.lsp", "loom.tools.agent"]:
    drop("loom.tools.runtime_registry", m)
# (b) blanket module renames (surviving importers rewritten automatically)
rename("loom.tools.agent", "loom.orchestration.agent")
rename("loom.tools.agent.run", "loom.orchestration.agent.run")
rename("loom.tools.agent.resume", "loom.orchestration.agent.resume")
rename("loom.tools.agent.fork", "loom.orchestration.agent.fork")
rename("loom.tools.agent.utils", "loom.orchestration.agent.utils")
rename("loom.tools.mcp", "loom.orchestration.tools.mcp")
rename("loom.tools.lsp", "loom.orchestration.tools.lsp")
rename("loom.tools.spawn_multi_agent", "loom.orchestration.agent.spawn_multi_agent")
# (c) bridge re-home: bootstrap module becomes orchestration module
kill("loom.bootstrap.mcp_connectivity")
newmod("loom.orchestration.mcp_connectivity", {
    "loom.hooks.remaining_notifs", "loom.services.mcp.types",
    "loom.services.mcp.connection_manager", "loom.orchestration.tools.mcp"})
# (d) orch runtime_backends gains lsp/mcp/computer_use backend bodies
for i in ["loom.orchestration.tools.mcp", "loom.orchestration.tools.lsp",
          "loom.orchestration.agent", "loom.services.image"]:
    add("loom.orchestration.runtime_backends", i)
# moved computer_use TU: services.image now reached from orch (already added),
# registry seam + mcp renamed intra-orch
add("loom.orchestration.runtime_backends", "loom.tools.agent_types")
# (e) seam leaf importer of tool_types (executor signatures) — rank down
add("loom.tools.runtime_backends.port", "loom.types.tool_types")
ok = gate_state("B15 B11-ATOMIC")

# ---- final area adjacency dump
tg, sccs = area_sccs9()
print("\n=== FINAL 9-area adjacency ===")
for a in gc.TARGET_AREAS:
    outs = sorted(tg.get(a, ()))
    print(f"  {a:18s} -> {outs}")

# ================= CMake target link graph =================
import pathlib, re
tdir = pathlib.Path("/home/zhangdi.zode/Develop/CC-REPL/src/cmake/targets")
def parse_links():
    edges = {}
    for cf in tdir.glob("*.cmake"):
        txt = cf.read_text()
        for m in re.finditer(r"target_link_libraries\(\s*([A-Za-z0-9_]+)(.*?)\)",
                             txt, re.S):
            tgt, body = m.group(1), m.group(2)
            for tok in re.findall(r"[A-Za-z0-9_:]+", body):
                if tok.startswith("loom_"):
                    edges.setdefault(tgt, set()).add(tok)
    # loom.cmake form
    return edges
links = parse_links()
cc = {t: s for t, s in links.items()}
cyc0 = [sorted(c) for c in gc.tarjan_scc(cc) if len(c) > 1]
print("\nLIVE cmake target link cycles:", cyc0 if cyc0 else "none")

# Modeled post-B15 link edits
cy = copy.deepcopy(cc)
cy["loom_orchestration"] = {"loom_tools", "loom_services", "loom_skills_core",
                          "loom_hooks", "loom_config", "loom_utils", "loom_types"}
cy["loom_tools"] = {"loom_utils", "loom_types", "loom_skills_core", "yyjson", "uv_a"}
cy["loom_bootstrap"] = {"loom_utils", "loom_state", "loom_config", "loom_services",
                      "loom_hooks"}
for t in ["loom_commands", "loom_server", "loom_ui"]:
    cy.setdefault(t, set()).add("loom_orchestration")
cy["loom_server"].discard  # noop
cy["loom_core"] = set(cy.get("loom_core", ())) | {"loom_orchestration"}
cy["loom_skills"].discard("loom_tools")
# only loom_* nodes matter
ccn = {t: {x for x in s if x.startswith("loom_")} for t, s in cy.items()}
cyc1 = [sorted(c) for c in gc.tarjan_scc(ccn) if len(c) > 1]
print("POST-B15 cmake target link cycles:", cyc1 if cyc1 else "none")
print("orchestration linkers:",
      sorted(t for t, s in ccn.items() if "loom_orchestration" in s))

sys.exit(0 if ok and not cyc0 and not cyc1 else 1)

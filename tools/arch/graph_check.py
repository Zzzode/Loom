#!/usr/bin/env python3
"""Loom architecture graph check (RFC 0001 milestone E0).

Builds the named-module import graph from src/ and enforces:

  CURRENT gate (default, runs in CI on every src change):
    1. module-level graph is a DAG;
    2. no NEW non-contract upward edge versus the frozen backlog in
       upward_edge_baseline.txt (additions fail; removals are fine);
    3. no NEW dead import versus dead_imports_baseline.txt: an imported
       module whose exported symbols/namespaces are never referenced in
       the importing file (additions fail; deleting dead imports shrinks
       the snapshot). Per-line silence for intentional imports:
       // arch-check: keep-import.

  FUTURE gate (--target-core8, the Phase B merge gate):
    the RFC 0001 target areas are pairwise acyclic — every one a singleton
    SCC (expected: 9 incl. orchestration). Fails today; passes after the
    14 families land.

  FUTURE gate (--target-ui9, RFC 0002 phase F0):
    the 12 loom.ui.<area> areas are pairwise acyclic — every one a singleton
    SCC — and no NEW back direction appears under the declared 12-area
    total order (ui_back_edge_baseline.txt). Fails today (2 SCCs: a
    7-area SCC + chrome<->foundation); passes after the RFC 0002 F1/F2
    cuts land. The default rank gate is blind to loom.ui-internal edges
    (loom.ui is one rank-12 area in TARGET_RANK), so this is their sole
    guard.

  STORE gate (--store-lint, RFC 0002 phase F3):
    the F3 state stores are homed in loom.ui.screens.* (rank 10). Four rules
    over src/ui/screens/*_store.cppm: (1) naming — a store file declares
    loom.ui.screens.<name>_store; (2) out-of-store — a store's loom.ui.*
    imports target only areas ranked BELOW screens (no store imports the
    composition root); (3) into-store — only app-area (composition root)
    or screens-area modules import a store; (4) threading — 0
    mutex/jthread/condition_variable tokens in a store module. Empty
    baseline: passes vacuously until the first store lands, and must pass
    from that commit on.

  TLL gate (--tll-lint, RFC 0002 phase F4):
    the target_link_libraries graph parsed from src/cmake/targets/*.cmake
    (the include()-per-target discipline) must be acyclic — 0 non-trivial
    SCCs and no self-loops. External libs (ftxui::*, OpenSSL::*, yyjson,
    ...) are leaf nodes. Once the loom_ui_<area> libraries land, the lint
    also enforces the file->lib grouping: every src/ui/**/*.cppm declaring
    loom.ui.<area>.* must be listed in the CXX_MODULES FILE_SET of
    loom_ui_<area> and in no other loom_ui_* target (nor loom_ui). Vacuous
    until the first area library appears (today all ui modules are in the
    single loom_ui target).

Zero third-party dependencies.
"""

from __future__ import annotations

import argparse
import glob
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
HERE = pathlib.Path(__file__).parent
ALLOWLIST = HERE / "port_allowlist.txt"
BASELINE = HERE / "upward_edge_baseline.txt"
DEAD_BASELINE = HERE / "dead_imports_baseline.txt"
UI9_BASELINE = HERE / "ui_back_edge_baseline.txt"

# Whole-text (multiline) forms applied AFTER comment/string stripping and
# backslash-newline joining, so legal spellings — `import\n loom.foo;`,
# `import /*c*/ loom.foo;`, `export /*c*/ import loom.foo;`, line splices —
# cannot hide an edge or an entire TU from the graph.
MODULE_DECL_RE = re.compile(
    r"\b(?:(export)\s+)?module\s+([A-Za-z0-9_.:]+)\s*;")
IMPORT_STMT_RE = re.compile(
    r"\b(?:(export)\s+)?import\s+(:?[A-Za-z0-9_][A-Za-z0-9_.:]*)\s*[;:]")
CPP_KEYWORDS = {
    "struct", "class", "enum", "union", "namespace", "using", "template",
    "typename", "typedef", "auto", "const", "constexpr", "consteval",
    "constinit", "inline", "static", "extern", "virtual", "explicit",
    "export", "public", "private", "protected", "friend", "operator",
    "return", "if", "else", "for", "while", "switch", "case", "default",
    "do", "break", "continue", "goto", "try", "catch", "throw", "new",
    "delete", "sizeof", "alignof", "requires", "concept", "co_await",
    "co_return", "co_yield", "true", "false", "nullptr", "this", "void",
    "bool", "char", "short", "int", "long", "float", "double", "signed",
    "unsigned", "noexcept", "override", "final", "mutable", "volatile",
    "concept", "requires",
}

# Target layer rank (RFC 0001 REV 3). Every live area must appear here so
# that no edge is silently skipped.
TARGET_RANK = {
    "loom.types": 0, "loom.constants": 0,
    "loom.wire": 0, "loom.core": 0,
    # coordinator types are pure-data leaves today.
    "loom.coordinator": 0,
    "loom.config": 1,
    "loom.migrations": 1,
    "loom.utils": 2,
    # RFC 0001 Phase D — loom.utils.* leaf-domain rename targets (rank 2,
    # same as loom.utils today; introduced batch-by-batch in the rename).
    "loom.agent": 2, "loom.cache": 2, "loom.containers": 2, "loom.crypto": 2,
    "loom.diagnostics": 2, "loom.fs": 2, "loom.media": 2, "loom.model": 2,
    "loom.net": 2,
    "loom.parsing": 2, "loom.platform": 2, "loom.process": 2, "loom.prompt": 2,
    "loom.scm": 2, "loom.security": 2,
    "loom.serdes": 2,
    "loom.text": 2,
    "loom.vim": 3,
    "loom.hooks": 4,
    "loom.skills": 5,
    "loom.state": 6, "loom.session": 6, "loom.history": 6,
    "loom.task_types": 6, "loom.memdir": 6, "loom.tasks": 6,
    "loom.services": 7,
    "loom.plugins": 7,
    # RFC 0001 Phase D B6 — loom_teams target (teams/swarm modules moved out
    # of loom_utils). Rank 7 (not 8) so that loom.tools (8) -> loom.teams (7) is
    # strictly downward.
    "loom.teams": 7,
    "loom.tools": 8,
    "loom.orchestration": 9,  # planned by RFC 0001 Phase B
    "loom.query": 10,
    "loom.commands": 11,
    "loom.keybindings": 11,
    "loom.ui": 12,
    "loom.server": 13, "loom.daemon": 13,
    "loom.bridge": 13,
    "loom.bootstrap": 13,
    "loom.cli": 14,
    "loom.sdk": 16,
    "loom.benchmarks": 15,
}

# Leaf modules physically located inside a higher-ranked area directory.
# They are standalone transport/util modules with no upward deps; rank them
# with utils so their importers do not acquire artificial upward edges.
MODULE_RANK_OVERRIDE = {
    "loom.cli.ccr_client": 2,
    "loom.cli.sse_transport": 2,
    "loom.cli.websocket_transport": 2,
    "loom.cli.update": 2,
    # RFC 0001 Phase D B5g (D5): the loom.config.settings_* modules import
    # loom.serdes.json (rank 2); loom.config is rank 1, so without the override
    # those edges read as rank-1->2 upward edges. Rank them with utils.
    "loom.config.settings_manager": 2,
    "loom.config.settings_merge": 2,
    "loom.config.settings_paths": 2,
    "loom.config.settings_sources": 2,
    "loom.config.settings_validation": 2,
}

# RFC 0001 Phase D B7: loom.utils stays in CORE8 even though the rename track
# dissolved the area down to one module — loom.utils.error (the D1 frozen
# exception, 52 importers across the other CORE8 areas). It is a pure leaf
# (imports only std), so it forms no SCC; keeping it here means the
# --target-core8 gate still checks its edges. Removing it would silently
# stop checking the area.
CORE8 = ["loom.config", "loom.hooks", "loom.services", "loom.skills",
         "loom.state", "loom.task_types", "loom.tools", "loom.utils"]
TARGET_AREAS = CORE8 + ["loom.orchestration"]

# RFC 0002 phase F0 — declared total order over the 12 loom.ui.<area> areas.
# "Back edge" is operational against this table: an area-direction A -> B is
# a back edge iff UI9_RANK[A] < UI9_RANK[B] (A imports a higher-ranked area).
# The order is the minimum-FAS order on the live graph (exhaustive 7! search
# over the 7-area SCC; gate package §0). visual/tools are pure leaves (zero
# loom.ui imports), so they rank below every importer; their mutual rank is
# irrelevant. After F2 severs all 5 back directions this table machine-
# enforces the declared order (0 upward edges allowed).
UI9_RANK = {
    "loom.ui.app": 11,
    "loom.ui.screens": 10,
    "loom.ui.dialogs": 9,
    "loom.ui.features": 8,
    "loom.ui.messages": 7,
    "loom.ui.permissions": 6,
    "loom.ui.widgets": 5,
    "loom.ui.prompt": 4,
    "loom.ui.chrome": 3,
    "loom.ui.foundation": 2,
    "loom.ui.visual": 1,
    "loom.ui.tools": 1,
}


def ui9_area_of(module: str):
    """The loom.ui.<area> second-level area (first 3 dot-segments), or None
    for modules outside loom.ui.*. Matches the attachment methodology: the
    core graph uses area_of() ([:2]); the UI9 flag uses [:3]."""
    parts = module.split(".")
    if len(parts) >= 3 and parts[0] == "loom" and parts[1] == "ui":
        return ".".join(parts[:3])
    return None


def load_ui9_baseline(path):
    """Parse ui_back_edge_baseline.txt into (sloom_internal, back) pair sets.

    The file holds two frozen sets in two comment-marked sections:
      `# [scc-internal]` — the frozen SCC-internal area-directions;
      `# [back]`          — the frozen back directions under UI9_RANK.
    A `from -> to` line lands in the set of the section it follows. Lines
    before any section header are ignored (the file header)."""
    p = pathlib.Path(path)
    if not p.exists():
        return set(), set()
    internal: set[tuple[str, str]] = set()
    back: set[tuple[str, str]] = set()
    current = None
    for ln in p.read_text().splitlines():
        s = ln.strip()
        if not s:
            continue
        if s.startswith("#"):
            header = s.lstrip("#").strip()
            if header == "[scc-internal]":
                current = internal
            elif header == "[back]":
                current = back
            continue
        if current is not None and " -> " in s:
            a, b = s.split(" -> ", 1)
            current.add((a.strip(), b.strip()))
    return internal, back


def ui9_check(deps):
    """RFC 0002 phase F0 — the --target-ui9 future-state gate.

    Two frozen sets, two checks over the 12-area loom.ui subgraph:
      (a) Tarjan + subset freeze — the SCC-internal area-directions are
          frozen at the baseline (19 today); a 20th fails. This is the
          sole guard for loom.ui-internal edges: the default rank gate is
          blind to them (loom.ui is one rank-12 area in TARGET_RANK).
      (b) Rank-based order conformance — under UI9_RANK, a direction
          A -> B with rank(A) < rank(B) is a back edge; any back direction
          not in the baseline (5 today) fails, including a NON-SCC-forming
          one (e.g. visual -> foundation passes (a) — visual is a
          singleton — but fails (b)). After F2 severs all 5, this enforces
          0 upward edges.

    Passes only when every area is a singleton SCC and neither frozen set
    gained an entry. Fails today (2 SCCs); passes after F2. Removals are
    fine and shrink the snapshot."""
    base_internal, base_back = load_ui9_baseline(UI9_BASELINE)

    # Fail closed: every loom.ui.<area> present in the graph must be ranked,
    # so no edge is silently skipped (same discipline as TARGET_RANK).
    unranked = sorted({
        a for m in deps if (a := ui9_area_of(m)) is not None
        and a not in UI9_RANK})

    g: dict[str, set[str]] = {a: set() for a in UI9_RANK}
    dir_edges: dict[tuple[str, str], set[tuple[str, str]]] = {}
    for m, imps in deps.items():
        a = ui9_area_of(m)
        if a is None:
            continue
        for i in imps:
            b = ui9_area_of(i)
            if b is not None and b != a:
                g[a].add(b)
                dir_edges.setdefault((a, b), set()).add((m, i))

    sccs = [sorted(c) for c in tarjan_scc(g) if len(c) > 1]

    internal: set[tuple[str, str]] = set()
    for c in sccs:
        cs = set(c)
        for a in c:
            for b in g.get(a, ()):
                if b in cs:
                    internal.add((a, b))

    back = {(a, b) for (a, b) in dir_edges if UI9_RANK[a] < UI9_RANK[b]}

    new_internal = sorted(internal - base_internal)
    removed_internal = sorted(base_internal - internal)
    new_back = sorted(back - base_back)
    removed_back = sorted(base_back - back)

    passes = (not sccs and not new_internal and not new_back
              and not unranked)
    return {
        "area_sccs": sccs,
        "unranked_areas": unranked,
        "sloom_internal": sorted(internal),
        "new_sloom_internal": new_internal,
        "removed_sloom_internal": removed_internal,
        "back": sorted(back),
        "new_back": new_back,
        "removed_back": removed_back,
        "dir_edges": {f"{a} -> {b}": sorted(edges)
                      for (a, b), edges in sorted(dir_edges.items())},
        "passes": passes,
    }


# RFC 0002 phase F3 — store placement invariant. The F3 state stores are
# homed in loom.ui.screens.* (rank 10): screens -> features/dialogs/prompt is
# downward-legal, so by-value concrete state fields (AgentCardData/
# LiveTeammate vectors, DialogQueue, ...) recreate no up-edge. Four rules:
#   naming   a src/ui/screens/*_store.cppm file declares
#            loom.ui.screens.<name>_store;
#   out      a store's loom.ui.* imports target only areas ranked BELOW screens
#            (UI9_RANK < 10) — no store imports the composition root (app)
#            or a same/higher area (a store importing another store is also
#            banned: cross-store reads go through selectors, never a direct
#            import);
#   in       a module importing a store is in the app area (composition
#            root) or the screens area (same-area) — no features/dialogs/
#            messages/prompt/... module reaches up to a store;
#   threads  0 mutex/jthread/condition_variable tokens in a store module —
#            stores are UI-thread-affined plain data; the one mutex in
#            repl_state (pending_at_mention_mutex) moves to AppImpl, not
#            into a store.
# Baseline: empty. The lint passes vacuously until the first store lands and
# must pass from that commit on (gate package section F3).
STORE_THREAD_RE = re.compile(r"\b(?:mutex|jthread|condition_variable)\b")


def _is_store_path(path) -> bool:
    """True for a file directly under src/ui/screens/ named *_store.cppm."""
    # Resolve both sides: a symlinked SRC (e.g. macOS /tmp -> /private/tmp
    # in the negative test's temp tree) must not silently drop a store.
    try:
        rel = pathlib.Path(path).resolve().relative_to(
            pathlib.Path(SRC).resolve())
    except ValueError:
        return False
    parts = rel.parts
    return (len(parts) == 3 and parts[0] == "ui" and parts[1] == "screens"
            and parts[2].endswith("_store.cppm"))


def store_lint_check(units, deps):
    """RFC 0002 phase F3 — the --store-lint placement gate (see STORE_*).

    Returns a dict with the four rule results and a flat `violations` list;
    `passes` is True iff every rule is clean. Fail-closed: a store importing
    an unranked loom.ui area, or a non-loom.ui module importing a store, is a
    violation rather than a silently skipped edge."""
    naming: list[str] = []
    out_of_store: list[tuple[str, str, str]] = []
    into_store: list[tuple[str, str]] = []
    threading: list[tuple[str, str]] = []

    store_units = [u for u in units if _is_store_path(u.path)]
    store_modules: set[str] = {u.module for u in store_units}
    for u in store_units:
        expected = "loom.ui.screens." + pathlib.Path(u.path).stem
        if u.module != expected:
            naming.append(
                f"{u.path}: declares {u.module}, expected {expected} "
                f"(a store file declares loom.ui.screens.<name>_store)")
        cleaned = _strip_comments_strings(u.text)
        for tok in sorted(set(STORE_THREAD_RE.findall(cleaned))):
            threading.append((u.module, tok))

    for m, imps in deps.items():
        if m in store_modules:
            for i in sorted(imps):
                if not i.startswith("loom.ui."):
                    continue
                area = ui9_area_of(i)
                if area is None or UI9_RANK.get(area, 99) >= 10:
                    out_of_store.append((m, i, area or "<unranked>"))
        for i in sorted(imps):
            if i in store_modules:
                area = ui9_area_of(m)
                if area not in ("loom.ui.app", "loom.ui.screens"):
                    into_store.append((m, i))

    violations = (
        [f"naming: {v}" for v in naming]
        + [f"store {m} imports rank>=screens area '{area}' via {i} "
           f"(store loom.ui.* imports must rank below screens)"
           for m, i, area in sorted(out_of_store)]
        + [f"{m} imports store {i} (only app-area or screens-area modules "
           f"may import a store)"
           for m, i in sorted(into_store)]
        + [f"store {m} uses threading token '{tok}' (stores are "
           f"UI-thread-affined plain data; mutexes live in AppImpl)"
           for m, tok in sorted(threading)])
    return {
        "stores": sorted(store_modules),
        "naming": naming,
        "out_of_store": [list(x) for x in sorted(out_of_store)],
        "into_store": [list(x) for x in sorted(into_store)],
        "threading": [list(x) for x in sorted(threading)],
        "violations": violations,
        "passes": not violations,
    }


# RFC 0002 phase F4 — target_link_libraries (TLL) graph lint. The F4 split
# turns the single loom_ui target into ~12 loom_ui_<area> libraries; the TLL
# graph must stay acyclic across the split. Two rules:
#   acyc   every target_link_libraries call in src/cmake/targets/*.cmake
#          contributes edges target -> dep; Tarjan must find 0 non-trivial
#          SCCs (and no self-loops). External libs (ftxui::screen,
#          OpenSSL::Crypto, yyjson, ...) are leaves with no outgoing edges.
#   group  once a loom_ui_<area> target exists, every src/ui/**/*.cppm
#          declaring loom.ui.<area>.* must be listed in that target's
#          CXX_MODULES FILE_SET and in no other loom_ui_* target (nor loom_ui);
#          symmetrically, a loom_ui_<area> FILE_SET must not list a .cppm
#          declaring another area. Vacuous until the first area library
#          lands (today all ui modules are in the single loom_ui target).
TLL_SCOPE_KEYWORDS = {
    "PUBLIC", "PRIVATE", "INTERFACE",
    "LINK_PUBLIC", "LINK_PRIVATE",
    "debug", "optimized", "general",
}
UI_AREA_TARGET_PREFIX = "loom_ui_"


def _strip_cmake_comments(text: str) -> str:
    """Blank CMake comments (# to EOL, #[[...]] bracket) with spaces,
    respecting double-quoted strings (positions preserved)."""
    out: list[str] = []
    i, n = 0, len(text)
    in_quote = False
    while i < n:
        c = text[i]
        if in_quote:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_quote = False
            i += 1
            continue
        if c == '"':
            in_quote = True
            out.append(c)
            i += 1
            continue
        if c == "#":
            if text.startswith("#[[", i):
                end = text.find("]]", i + 3)
                i = n if end == -1 else end + 2
            else:
                eol = text.find("\n", i)
                i = n if eol == -1 else eol
            out.append(" ")
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _cmake_call_bodies(text: str, command: str) -> list[str]:
    """The argument body of every `command(...)` call in comment-stripped
    text. Parens balance; quoted strings are skipped so a ')' or '#' inside
    one cannot end the call early."""
    bodies: list[str] = []
    for m in re.finditer(r"\b" + re.escape(command) + r"\s*\(", text,
                         re.IGNORECASE):
        depth, j, n = 1, m.end(), len(text)
        in_quote = False
        while j < n and depth > 0:
            c = text[j]
            if in_quote:
                if c == '"':
                    in_quote = False
                j += 1
                continue
            if c == '"':
                in_quote = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        bodies.append(text[m.end():j])
    return bodies


def _tokenize_cmake_args(body: str) -> list[tuple[str, str]]:
    """Split a CMake argument body into (kind, value) tokens. kind is
    'bare' or 'quoted' (quoted values have the quotes stripped)."""
    tokens: list[tuple[str, str]] = []
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c.isspace():
            i += 1
            continue
        if c == '"':
            j = i + 1
            buf: list[str] = []
            while j < n and body[j] != '"':
                if body[j] == "\\" and j + 1 < n:
                    buf.append(body[j + 1])
                    j += 2
                    continue
                buf.append(body[j])
                j += 1
            tokens.append(("quoted", "".join(buf)))
            i = j + 1
            continue
        j = i
        while j < n and not body[j].isspace():
            j += 1
        tokens.append(("bare", body[i:j]))
        i = j
    return tokens


def parse_tll_calls(text: str) -> list[tuple[str, list[str]]]:
    """Parse every target_link_libraries(<target> ...) call into
    (target, deps). Handles the multi-line PUBLIC/PRIVATE/INTERFACE form,
    inline comments, and quoted linker flags (skipped — never targets).
    Multiple calls for one target accumulate (CMake semantics)."""
    text = _strip_cmake_comments(text)
    calls: list[tuple[str, list[str]]] = []
    for body in _cmake_call_bodies(text, "target_link_libraries"):
        toks = _tokenize_cmake_args(body)
        if not toks or toks[0][0] != "bare":
            continue
        target = toks[0][1]
        deps = [val for kind, val in toks[1:]
                if kind == "bare" and val not in TLL_SCOPE_KEYWORDS]
        calls.append((target, deps))
    return calls


def parse_cxx_module_filesets(text: str) -> dict[str, list[str]]:
    """Parse `target_sources(<target> ... FILE_SET CXX_MODULES FILES ...)`
    calls into {target: [listed files]}. Only the CXX_MODULES file set is
    harvested (PRIVATE .cpp impl units are not module interfaces)."""
    text = _strip_cmake_comments(text)
    out: dict[str, list[str]] = {}
    for body in _cmake_call_bodies(text, "target_sources"):
        toks = [v for k, v in _tokenize_cmake_args(body) if k == "bare"]
        if not toks:
            continue
        target = toks[0]
        i = 1
        while i < len(toks):
            if (toks[i] == "FILE_SET" and i + 1 < len(toks)
                    and toks[i + 1] == "CXX_MODULES"):
                j = i + 2
                # Skip BASE_DIRS <dirs>... until the FILES keyword.
                while (j < len(toks) and toks[j] != "FILES"
                       and toks[j] != "FILE_SET"):
                    j += 1
                if j < len(toks) and toks[j] == "FILES":
                    j += 1
                    while j < len(toks) and toks[j] != "FILE_SET":
                        out.setdefault(target, []).append(toks[j])
                        j += 1
                i = j
                continue
            i += 1
    return out


def tll_lint_check(units):
    """RFC 0002 phase F4 — the --tll-lint gate (see the TLL notes above).

    Returns a dict with the graph, the SCC/self-loop findings, and the
    file->lib grouping violations; `passes` is True iff both rules are
    clean. The grouping rule activates per-area: a constraint fires only
    for areas whose loom_ui_<area> target exists, so the lint is vacuous
    until the first area library lands."""
    targets_dir = SRC / "cmake" / "targets"
    graph: dict[str, set[str]] = {}
    tll_calls = 0
    filesets: dict[str, set[str]] = {}
    if targets_dir.is_dir():
        for cf in sorted(targets_dir.glob("*.cmake")):
            text = cf.read_text(encoding="utf-8", errors="ignore")
            for target, deps in parse_tll_calls(text):
                tll_calls += 1
                graph.setdefault(target, set()).update(deps)
                for d in deps:
                    graph.setdefault(d, set())
            for target, files in parse_cxx_module_filesets(text).items():
                filesets.setdefault(target, set()).update(files)

    sccs = [sorted(c) for c in tarjan_scc(graph) if len(c) > 1]
    self_loops = sorted(t for t, deps in graph.items() if t in deps)

    # Rule (b): file->lib grouping.
    area_targets = {t[len(UI_AREA_TARGET_PREFIX):]: t
                    for t in filesets
                    if t.startswith(UI_AREA_TARGET_PREFIX)}
    src_resolved = pathlib.Path(SRC).resolve()

    def _norm_listed(p: str) -> str:
        rp = (pathlib.Path(SRC) / p).resolve()
        try:
            return rp.relative_to(src_resolved).as_posix()
        except ValueError:
            return p.replace("\\", "/")

    norm_filesets: dict[str, set[str]] = {
        t: {_norm_listed(f) for f in fs} for t, fs in filesets.items()}

    grouping: list[str] = []
    if area_targets:
        # Which loom_ui/loom_ui_* target lists each ui file.
        ownership: dict[str, set[str]] = {}
        for t, fs in norm_filesets.items():
            if t == "loom_ui" or t.startswith(UI_AREA_TARGET_PREFIX):
                for f in fs:
                    ownership.setdefault(f, set()).add(t)
        # File-side: a split-area module must be homed in its area library.
        for u in units:
            if not u.path.endswith(".cppm"):
                continue
            area = ui9_area_of(u.module)
            if area is None:
                continue
            try:
                rel = pathlib.Path(u.path).resolve().relative_to(
                    src_resolved).as_posix()
            except ValueError:
                continue
            expected = UI_AREA_TARGET_PREFIX + area.split(".")[2]
            if expected not in norm_filesets:
                continue  # area not split yet — the file stays in loom_ui
            owners = ownership.get(rel, set())
            if expected not in owners:
                grouping.append(
                    f"{rel} ({u.module}) must be in {expected}'s "
                    f"CXX_MODULES FILE_SET (found in: "
                    f"{sorted(owners) if owners else 'no loom_ui* target'})")
            for t in sorted(owners - {expected}):
                grouping.append(
                    f"{rel} ({u.module}) is listed in {t}'s FILE_SET but "
                    f"its area is {area} (only {expected} may own it)")
        # Target-side: a loom_ui_<area> FILE_SET must not list another area's
        # module (catches a not-yet-split area's file misplaced into a
        # split library, which the file-side check cannot see).
        module_by_path: dict[str, str] = {}
        for u in units:
            if not u.path.endswith(".cppm"):
                continue
            try:
                rel = pathlib.Path(u.path).resolve().relative_to(
                    src_resolved).as_posix()
            except ValueError:
                continue
            module_by_path[rel] = u.module
        for area, t in sorted(area_targets.items()):
            for f in sorted(norm_filesets.get(t, ())):
                mod = module_by_path.get(f)
                if mod is None:
                    continue
                fa = ui9_area_of(mod)
                if fa is not None and fa != "loom.ui." + area:
                    grouping.append(
                        f"{t} lists {f} which declares {mod} (area {fa}); "
                        f"a loom_ui_<area> FILE_SET may list only "
                        f"loom.ui.{area}.* modules")

    passes = not sccs and not self_loops and not grouping
    return {
        "targets": sorted(graph),
        "links": {t: sorted(d) for t, d in sorted(graph.items())},
        "tll_calls": tll_calls,
        "sccs": sccs,
        "self_loops": self_loops,
        "area_targets": sorted(area_targets.values()),
        "grouping": grouping,
        "passes": passes,
    }


def area_of(module: str) -> str:
    parts = module.split(".")
    return ".".join(parts[:2]) if len(parts) >= 2 else module


def rank_of(module: str):
    if module in MODULE_RANK_OVERRIDE:
        return MODULE_RANK_OVERRIDE[module]
    return TARGET_RANK.get(area_of(module))


def is_contract(module: str, allow: set[str]) -> bool:
    """Structural test — exact-segment, never a substring match."""
    leaf = module.split(".")[-1]
    if leaf in ("port", "contract"):
        return True
    if leaf.endswith("_types"):
        return True
    if module == "loom.types" or module.startswith("loom.types."):
        return True
    if module in allow:
        return True
    return False


def load_allowlist() -> set[str]:
    if not ALLOWLIST.exists():
        return set()
    return {ln.strip() for ln in ALLOWLIST.read_text().splitlines()
            if ln.strip() and not ln.strip().startswith("#")}


def load_pair_baseline(path, sep: str) -> set[tuple[str, str]]:
    p = pathlib.Path(path)
    if not p.exists():
        return set()
    out = set()
    for ln in p.read_text().splitlines():
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        if sep in ln:
            a, b = ln.split(sep, 1)
            out.add((a.strip(), b.strip()))
    return out


def load_baseline() -> set[tuple[str, str]]:
    return load_pair_baseline(BASELINE, " -> ")


# A file in the module graph. kind: primary | partition | impl
class Unit:
    __slots__ = ("module", "kind", "text", "path", "imports", "reexports")

    def __init__(self, module, kind, text, path):
        self.module = module
        self.kind = kind
        self.text = text
        self.path = path
        self.imports: set[str] = set()
        self.reexports: set[str] = set()


def _normalize_for_scan(text: str) -> str:
    """Phase-2 backslash-newline splices joined (tokens concatenate), then
    comments/strings blanked (positions kept), then preprocessor directives
    blanked (so `<import>`-style include names can't match).

    The stripper is raw-string-naive, but C++ requires import declarations
    to immediately follow the module declaration, so a raw literal cannot
    appear before an import — it cannot hide an edge in practice."""
    text = re.sub(r"\\\r?\n", "", text)
    text = _strip_comments_strings(text)
    text = re.sub(r"(?m)^[ \t]*#.*$", "", text)
    return text


def load_units():
    units: list[Unit] = []
    paths = glob.glob(str(SRC / "**" / "*.cppm"), recursive=True)
    paths += glob.glob(str(SRC / "**" / "*.cpp"), recursive=True)
    for path in paths:
        raw_text = pathlib.Path(path).read_text(
            encoding="utf-8", errors="ignore")
        text = _normalize_for_scan(raw_text)
        m = MODULE_DECL_RE.search(text)
        if not m:
            continue
        mod_raw = m.group(2)
        if ":" in mod_raw:
            kind = "partition"
        elif m.group(1):
            kind = "primary"
        else:
            kind = "impl"
        name = mod_raw.split(":")[0]
        unit = Unit(name, kind, raw_text, path)
        for sm in IMPORT_STMT_RE.finditer(text):
            imp = sm.group(2).split(":")[0]
            if imp.startswith("loom."):
                unit.imports.add(imp)
                if sm.group(1):
                    unit.reexports.add(imp)
        units.append(unit)
    known = {u.module for u in units}
    for u in units:
        u.imports = {i for i in u.imports if i in known}
        u.reexports = {i for i in u.reexports if i in known}
    return units


def module_deps(units):
    deps: dict[str, set[str]] = {}
    for u in units:
        deps.setdefault(u.module, set()).update(u.imports)
    return deps


def tarjan_scc(graph: dict[str, set[str]]) -> list[list[str]]:
    index, low, stack, on = {}, {}, [], set()
    counter, out = [0], []

    def visit(v):
        index[v] = low[v] = counter[0]; counter[0] += 1
        stack.append(v); on.add(v)
        for w in graph.get(v, ()):
            if w not in index:
                visit(w); low[v] = min(low[v], low[w])
            elif w in on:
                low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            comp = []
            while True:
                w = stack.pop(); on.discard(w); comp.append(w)
                if w == v:
                    break
            out.append(comp)

    sys.setrecursionlimit(100000)
    for v in graph:
        if v not in index:
            visit(v)
    return out


def area_graph(deps):
    g = {}
    for m, imps in deps.items():
        a = area_of(m)
        bk = g.setdefault(a, set())
        for i in imps:
            b = area_of(i)
            if b != a:
                bk.add(b)
    return g


def upward_edges(deps, allow):
    out = []
    for m in sorted(deps):
        ra = rank_of(m)
        if ra is None:
            continue
        for i in sorted(deps[m]):
            rb = rank_of(i)
            if rb is not None and rb > ra:
                out.append((m, i, is_contract(i, allow)))
    return out


def _strip_comments_strings(text: str) -> str:
    """Replace comments/string contents with spaces (positions preserved)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" "); i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            while i + 1 < n and (text[i], text[i + 1]) != ("*", "/"):
                out.append(" " if text[i] != "\n" else "\n"); i += 1
            i += 2; out.extend("  ")
        elif c in "\"'":
            quote = c; out.append(" "); i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    out.append(" "); i += 1
                    if i < n: out.append(" "); i += 1
                else:
                    out.append(" " if text[i] != "\n" else "\n"); i += 1
            if i < n: out.append(" "); i += 1
        else:
            out.append(c); i += 1
    return "".join(out)


def _parse_interface(text: str, exported_only: bool):
    """Return (names, namespace_paths) declared by one file.

    Style of this tree: `export namespace cc::x { ...plain decls... }`.
    A region stack distinguishes declaration scopes (namespace/class/enum)
    from function bodies and initializers, so call sites like push_back()
    are never mistaken for declarations. Member VARIABLES are deliberately
    not harvested: names like `id`/`string` collide with ubiquitous tokens
    and would mask genuinely unused imports.
    """
    bucket: set[str] = set()
    nsb: set[str] = set()
    text = _strip_comments_strings(text)

    token_re = re.compile(
        r"\b(struct|class|enum|union|namespace|using)\b"
        r"|([A-Za-z_]\w*)\s*\("
        r"|([A-Za-z_]\w*)"
        r"|([{};])")
    kind_re = re.compile(r"\b(namespace|struct|class|enum|union)\b")

    # Stack entries: (kind in {ns,cls,en,other,block}, exported, ns_name)
    stack: list[tuple[str, bool, str]] = [("other", not exported_only, "")]
    pending_export = False
    last_boundary = -1

    def head_exported():
        return pending_export or stack[-1][1]

    def current_ns_path():
        segs: list[str] = []
        for _, _, ns in stack:
            if ns:
                segs.extend(ns.split("::"))
        return "::".join(segs)

    i, n = 0, len(text)
    while i < n:
        m = token_re.match(text, i)
        if not m:
            i += 1
            continue
        i = m.end()

        if m.group(1):  # declaration keyword
            kw = m.group(1)
            if kw == "using":
                rest = text[m.end():m.end() + 200]
                um = re.match(r"\s*(?!enum\b)([A-Za-z_]\w*)\s*=", rest)
                if um and head_exported():
                    bucket.add(um.group(1))
            elif kw == "namespace":
                rest = text[m.end():m.end() + 300]
                if "=" in rest.split(";", 1)[0] or "{" not in rest.split(";", 1)[0]:
                    # namespace ALIAS (`namespace fs = ...;`) or forward form
                    # without a body here — not a namespace path we own.
                    pass
                else:
                    nm = re.match(r"\s*(?:inline\s+)?([A-Za-z_][\w:]*)", rest)
                    if nm:
                        nsb.add(nm.group(1))
                # nested-namespace composition is finalized at the brace
            continue

        brace = m.group(4)
        if brace == "{":
            head = text[last_boundary + 1:m.start()]
            km = kind_re.search(head)
            if km:
                word = km.group(1)
                kind = {"namespace": "ns", "enum": "en"}.get(word, "cls")
                frame_ns = ""
                if word == "namespace":
                    nm = re.search(
                        r"namespace\s+(?:inline\s+)?([A-Za-z_][\w:]*)", head)
                    if nm:
                        frame_ns = nm.group(1)
                        nsb.add("::".join(
                            p for p in (current_ns_path(), frame_ns) if p))
                else:
                    nm = re.search(
                        r"\b(?:struct|class|enum(?:\s+class)?|union)\s+"
                        r"([A-Za-z_]\w*)", head)
                    if nm and head_exported():
                        bucket.add(nm.group(1))
                stack.append((kind, head_exported(), frame_ns))
            elif re.search(r"\bexport\s*$", head):
                stack.append(("block", True, ""))  # `export { ... }`
            else:
                # Function/control/initializer body: locals are not interface.
                stack.append(("other", False, ""))
            pending_export = False
            last_boundary = m.start()
            continue
        if brace == "}":
            if len(stack) > 1:
                stack.pop()
            last_boundary = m.start()
            continue
        if brace == ";":
            head = text[last_boundary + 1:m.start()]
            stripped = head.strip()
            # The head ending in the module declaration carries the whole
            # global module fragment (#include lines); never harvest names
            # from it or from module/import/preprocessor statements.
            is_module_stmts = bool(
                re.search(r"(?:^|\n)\s*(?:export\s+)?(?:module|import)\b",
                          stripped)
                or re.search(r"(?:^|\n)\s*#", stripped)
                or (re.search(r"\bnamespace\b", stripped)
                    and "(" not in stripped))
            top_kind, top_exp, _ = stack[-1]
            exported_stmt = pending_export or (
                top_exp and top_kind in ("ns", "block"))
            local_member = (not exported_only and top_kind == "cls")
            if (exported_stmt or local_member) and not is_module_stmts:
                for f in re.findall(r"([A-Za-z_]\w*)\s*\(", head):
                    if f not in CPP_KEYWORDS:
                        bucket.add(f.lstrip("~"))
                if "(" not in head and "::" not in head:
                    # namespace-scope variable/alias. A qualified
                    # using-declaration (`using cc::x::Name;`) is a REFERENCE
                    # to another module, not a local declaration.
                    tail = re.split(r"[=]", head, maxsplit=1)[0]
                    ids = re.findall(r"[A-Za-z_]\w*", tail)
                    for v in ids[-2:]:
                        if v not in CPP_KEYWORDS:
                            bucket.add(v)
            pending_export = False
            last_boundary = m.start()
            continue

        name_tok = m.group(3)
        if name_tok == "export":
            pending_export = True
            continue
        if name_tok in CPP_KEYWORDS:
            continue
        if m.group(2):  # function/ctor declarator at ns/class/block scope
            top = stack[-1]
            if top[1] and top[0] in ("ns", "cls", "block"):
                bucket.add(m.group(2).lstrip("~"))
            continue
        if stack[-1][0] == "en" and stack[-1][1]:
            bucket.add(name_tok)  # enumerator

    return bucket, nsb


def exported_symbols(units) -> tuple[dict[str, set[str]], dict[str, set[str]]]:
    """Per module: (exported names, declared namespace paths).
    Union over primary + partitions — the module interface."""
    names: dict[str, set[str]] = {}
    ns_paths: dict[str, set[str]] = {}
    for u in units:
        if u.kind == "impl":
            continue
        bucket, nsb = _parse_interface(u.text, exported_only=True)
        names.setdefault(u.module, set()).update(bucket)
        ns_paths.setdefault(u.module, set()).update(nsb)
    return names, ns_paths




def _prefix_chain(body: str, pos: int) -> str:
    """The `a::b::c::` qualifier chain immediately before position pos,
    or "" if the name is not namespace-qualified."""
    i = pos
    segs: list[str] = []
    while i >= 2 and body[i - 2:i] == "::":
        j = i - 2
        k = j
        while k > 0 and (body[k - 1].isalnum() or body[k - 1] == "_"):
            k -= 1
        if k == j:
            return ""
        segs.append(body[k:j])
        i = k
    return "::".join(reversed(segs))


def unit_label(u: Unit) -> str:
    """Stable per-translation-unit identity. Implementation units sharing a
    module name MUST be distinguished by file (one TU may need an import a
    sibling TU does not)."""
    if u.kind == "impl":
        rel = pathlib.Path(u.path).resolve().relative_to(SRC)
        return f"{u.module} [impl:{rel}]"
    if u.kind == "partition":
        return f"{u.module}:{pathlib.Path(u.path).stem}"
    return u.module


def find_dead_imports(units, symbols, ns_paths):
    """An import is dead when NONE of the imported module's exported names
    are referenced in the importing FILE. A candidate name is evidence of
    use only if it (a) is not shadowed by a declaration in this file or the
    module's own interface, and (b) has an unqualified occurrence —
    `::name`/`.name` are member/qualified accesses (genuine qualified uses
    are matched through the namespace paths or a namespace alias)."""
    # namespace path -> set of owning modules. Namespaces are OPEN in C++:
    # several modules may declare the same namespace (e.g. repl_state and
    # repl_screen both open cc::ui::repl_screen), so ownership is a set and
    # iteration order must never decide the result.
    ns_owner: dict[str, set[str]] = {}
    for mod, paths in ns_paths.items():
        for p in paths:
            ns_owner.setdefault(p, set()).add(mod)

    alias_re = re.compile(
        r"\bnamespace\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_][\w:]*)\s*;")

    # Namespace aliases declared ANYWHERE in a module are visible in every
    # unit of that module: an impl unit may say `repl::Foo` where the
    # primary declares `namespace repl = cc::ui::repl_screen;`. It must
    # still import the aliased module itself (alias gives no visibility).
    module_aliases: dict[str, dict[str, str]] = {}
    for v in units:
        bk = module_aliases.setdefault(v.module, {})
        for am in alias_re.finditer(v.text):
            bk[am.group(1)] = am.group(2)

    dead = []
    for u in units:
        kept = set()
        file_silenced = False
        body_lines = []
        pending_keep = False
        for line in u.text.splitlines():
            if "arch-check: keep-imports" in line:
                file_silenced = True
            if "arch-check: keep-import" in line:
                mm = re.search(r"import\s+([A-Za-z0-9_][A-Za-z0-9_.:]*)", line)
                if mm:
                    kept.add(mm.group(1).split(":")[0])
                else:
                    pending_keep = True  # marker on its own line
            elif pending_keep:
                mm = re.search(r"import\s+([A-Za-z0-9_][A-Za-z0-9_.:]*)", line)
                if mm:
                    kept.add(mm.group(1).split(":")[0])
                pending_keep = False
            if not line.lstrip().startswith(("import ", "export import", "//", "*")):
                body_lines.append(line)
        if file_silenced:
            continue
        # NOTE: body is raw text minus full-line comments/imports. Names in
        # trailing comments or string literals still count as use — a
        # false-NEGATIVE-only bias (real dead imports may be missed, but a
        # live import is never wrongly flagged). Tightening this is a
        # separate step that needs its own compile-verified snapshot.
        body = "\n".join(body_lines)
        # Declarations in this file or the module's own interface shadow
        # imported names; sibling implementation units do NOT (per-TU).
        own, _ = _parse_interface(u.text, exported_only=False)
        own |= symbols.get(u.module, set())

        # Namespace aliases visible in this TU: declared here or in any
        # other unit of this module (see module_aliases above).
        aliases = dict(module_aliases.get(u.module, {}))

        for imp in sorted(u.imports):
            if imp in u.reexports or imp in kept:
                continue
            used = False
            for n in symbols.get(imp, ()):
                if not n or n in own:
                    continue
                for mm in re.finditer(rf"\b{re.escape(n)}\b", body):
                    prev = body[mm.start() - 1] if mm.start() > 0 else ""
                    if prev == ".":
                        continue  # member access can't name a free symbol
                    if prev == ":":
                        # `Q::name` counts only when Q is a namespace declared
                        # by the imported module (free function in that
                        # namespace) — never for `object.method`-style
                        # qualification or other modules' scopes.
                        chain = _prefix_chain(body, mm.start())
                        if not chain or not any(
                                chain == p or chain.startswith(p + "::")
                                for p in ns_paths.get(imp, ())):
                            continue
                    used = True
                    break
                if used:
                    break
            if not used:
                # namespace-qualified use: full `cc::area::sub::Foo`, a
                # relative `sub::Foo`, or an aliased `dt::Foo`. Paths shorter
                # than 3 segments (cc::utils, cc::core) are NOT evidence: many
                # modules share the area namespace, so text naming another
                # module in the same area would otherwise match spuriously.
                for path in ns_paths.get(imp, ()):
                    if path.count("::") < 2:
                        continue
                    if path in body:
                        used = True
                        break
                    seg = path.rsplit("::", 1)[-1]
                    if re.search(rf"\b{re.escape(seg)}::", body):
                        used = True
                        break
                if not used:
                    for alias, target in aliases.items():
                        owners: set[str] = set()
                        for p, mods in ns_owner.items():
                            # Only DEEP paths identify an owning module; an
                            # area-level path (cc::ui) is opened by dozens of
                            # modules and would match every target under it.
                            if p.count("::") < 2:
                                continue
                            if target == p or target.startswith(p + "::"):
                                owners |= mods
                        if imp in owners and re.search(
                                rf"\b{re.escape(alias)}::", body):
                            used = True
                            break
            if not used and imp in body:  # dotted form (validated snapshot)
                used = True
            if not used:
                dead.append((unit_label(u), imp))
    return dead


def run(target, allow_dead_imports=False, target_ui9=False, store_lint=False,
        tll_lint=False):
    allow = load_allowlist()
    baseline = load_baseline()
    dead_baseline = load_pair_baseline(DEAD_BASELINE, " -> ")
    units = load_units()
    deps = module_deps(units)
    symbols, ns_paths = exported_symbols(units)

    # Fail closed: every area present in the graph must have a rank. A new
    # unranked area would otherwise have all its upward edges silently
    # skipped. (Per-module overrides only relocate individual leaves.)
    unranked = sorted({
        area_of(m) for m in deps if area_of(m) not in TARGET_RANK})

    cycles = [c for c in tarjan_scc(deps) if len(c) > 1]
    up = upward_edges(deps, allow)
    illegal = [(m, i) for m, i, ok in up if not ok]
    new_edges = sorted(set(illegal) - baseline)
    removed = sorted(baseline - set(illegal))
    dead = find_dead_imports(units, symbols, ns_paths)
    new_dead = sorted(set(dead) - dead_baseline)
    dead_removed = sorted(dead_baseline - set(dead))

    areas = area_graph(deps)
    area_sccs = sorted((sorted(c) for c in tarjan_scc(areas) if len(c) > 1),
                       key=len, reverse=True)

    report = {
        "modules": len(deps),
        "units": len(units),
        "module_cycles": cycles,
        "illegal_upward_edges": illegal,
        "new_upward_edges": new_edges,
        "baseline_edges_removed": removed,
        "dead_imports": dead,
        "new_dead_imports": new_dead,
        "dead_baseline_removed": dead_removed,
        "area_sccs_current": area_sccs,
        "unranked_areas": unranked,
    }

    if target:
        tg = {a: set() for a in TARGET_AREAS}
        for m, imps in deps.items():
            a = area_of(m)
            if a not in tg:
                continue
            for i in imps:
                b = area_of(i)
                if b in tg and b != a:
                    tg[a].add(b)
        sccs = [sorted(c) for c in tarjan_scc(tg) if len(c) > 1]
        report["target_area_sccs"] = sccs
        report["target_core8_passes"] = not sccs

    if target_ui9:
        report["ui9"] = ui9_check(deps)

    if store_lint:
        report["store_lint"] = store_lint_check(units, deps)

    if tll_lint:
        report["tll_lint"] = tll_lint_check(units)

    # Default gate fails on: unranked area, module cycles, NEW upward
    # edges, NEW dead imports. Known baseline backlog is allowed.
    current_ok = (not unranked and not cycles and not new_edges
                  and (not new_dead or allow_dead_imports))
    report["passes"] = current_ok and (
        not target or report.get("target_core8_passes", True))
    if target_ui9:
        report["passes"] = report["passes"] and report["ui9"]["passes"]
    if store_lint:
        report["passes"] = report["passes"] and report["store_lint"]["passes"]
    if tll_lint:
        report["passes"] = report["passes"] and report["tll_lint"]["passes"]
    return report


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target-core8", action="store_true")
    ap.add_argument("--target-ui9", action="store_true",
                    help="RFC 0002 F0 future-state gate: the 12 loom.ui.<area> "
                         "areas must be singleton SCCs and no NEW back "
                         "direction may appear under the declared 12-area "
                         "total order")
    ap.add_argument("--store-lint", action="store_true",
                    help="RFC 0002 F3 store placement gate: stores homed in "
                         "loom.ui.screens.*, imports only from below-screens "
                         "areas, importers only app/screens, no threading "
                         "primitives")
    ap.add_argument("--tll-lint", action="store_true",
                    help="RFC 0002 F4 TLL graph gate: the "
                         "target_link_libraries graph from "
                         "src/cmake/targets/*.cmake must be acyclic (0 "
                         "SCCs, no self-loops); once loom_ui_<area> libraries "
                         "exist, every loom.ui.<area>.* module must be homed "
                         "in its area library's CXX_MODULES FILE_SET")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--allow-dead-imports", action="store_true",
                    help="tolerate NEW dead imports too (escape hatch)")
    args = ap.parse_args()
    r = run(args.target_core8, args.allow_dead_imports, args.target_ui9,
            args.store_lint, args.tll_lint)

    gate_ok = r["passes"]

    if args.json:
        print(json.dumps(r, indent=2))
        return 0 if gate_ok else 1

    print(f"modules analyzed: {r['modules']} ({r['units']} units)")
    print(f"module-level cycles: {len(r['module_cycles'])}")
    for c in r["module_cycles"][:10]:
        print("  CYCLE:", " -> ".join(sorted(c)[:8]))
    if r["unranked_areas"]:
        print("UNRANKED AREAS (add them to TARGET_RANK) — FAIL:")
        for a in r["unranked_areas"]:
            print(f"    {a}")
    print(f"non-contract upward edges: {len(r['illegal_upward_edges'])} "
          f"(baseline backlog; {len(r['baseline_edges_removed'])} removed)")
    if r["new_upward_edges"]:
        print("  NEW (not in baseline) — FAIL:")
        for m, i in r["new_upward_edges"]:
            print(f"    {m} -> {i}")
    if r["baseline_edges_removed"]:
        print("  removed since baseline (good):")
        for m, i in r["baseline_edges_removed"][:20]:
            print(f"    {m} -> {i}")
    print(f"dead imports: {len(r['dead_imports'])} (baseline backlog; "
          f"{len(r['dead_baseline_removed'])} removed)")
    if r["new_dead_imports"]:
        print("  NEW (not in baseline) — FAIL:")
        for m, i in r["new_dead_imports"][:40]:
            print(f"    {m} -> {i}  (delete it, or // arch-check: keep-import)")
    print(f"current directory SCCs (>1): {len(r['area_sccs_current'])}")
    for c in r["area_sccs_current"]:
        print("  SCC:", ", ".join(c))
    if args.target_core8:
        print("RFC 0001 Phase B target (9 singleton areas):",
              "PASS" if r["target_core8_passes"] else "FAIL")
        for c in r["target_area_sccs"]:
            print("  TARGET SCC:", ", ".join(c))
    if args.target_ui9:
        u = r["ui9"]
        print("RFC 0002 F0 target (12 singleton loom.ui areas):",
              "PASS" if u["passes"] else "FAIL")
        for c in u["area_sccs"]:
            print("  TARGET UI9 SCC:", ", ".join(c))
        if u["unranked_areas"]:
            print("  UNRANKED loom.ui AREAS (add them to UI9_RANK) — FAIL:")
            for a in u["unranked_areas"]:
                print(f"    {a}")
        print(f"  SCC-internal directions: {len(u['sloom_internal'])} "
              f"(frozen baseline; {len(u['removed_sloom_internal'])} removed)")
        if u["new_sloom_internal"]:
            print("  NEW SCC-internal direction (not in baseline) — FAIL:")
            for a, b in u["new_sloom_internal"]:
                print(f"    {a} -> {b}")
        if u["removed_sloom_internal"]:
            print("  removed since baseline (good):")
            for a, b in u["removed_sloom_internal"]:
                print(f"    {a} -> {b}")
        print(f"  back directions under rank table: {len(u['back'])} "
              f"(frozen baseline; {len(u['removed_back'])} removed)")
        if u["new_back"]:
            print("  NEW back direction (not in baseline) — FAIL:")
            for a, b in u["new_back"]:
                print(f"    {a} -> {b}")
                for m, i in u["dir_edges"].get(f"{a} -> {b}", []):
                    print(f"      {m} -> {i}")
        if u["removed_back"]:
            print("  removed since baseline (good):")
            for a, b in u["removed_back"]:
                print(f"    {a} -> {b}")
    if args.store_lint:
        s = r["store_lint"]
        print("RFC 0002 F3 store placement lint:",
              "PASS" if s["passes"] else "FAIL")
        print(f"  stores: {len(s['stores'])}"
              + (f" ({', '.join(s['stores'])})" if s["stores"] else ""))
        for v in s["violations"]:
            print(f"  STORE LINT: {v}")
    if args.tll_lint:
        t = r["tll_lint"]
        if t["passes"]:
            print(f"RFC 0002 F4 TLL lint: PASS — {len(t['targets'])} "
                  f"targets, 0 SCCs")
        else:
            print("RFC 0002 F4 TLL lint: FAIL")
        print(f"  TLL calls parsed: {t['tll_calls']} "
              f"({len(t['links'])} graph nodes)")
        for c in t["sccs"]:
            print("  TLL SCC:", " -> ".join(c))
        for tgt in t["self_loops"]:
            print(f"  TLL SELF-LOOP: {tgt} links itself")
        for v in t["grouping"]:
            print(f"  TLL GROUPING: {v}")

    print("graph_check:", "OK" if gate_ok else "FAIL")
    return 0 if gate_ok else 1


if __name__ == "__main__":
    sys.exit(main())

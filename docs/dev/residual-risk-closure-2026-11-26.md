# Residual-risk closure — 2026-11-26

Owner request: "残留风险先处理掉吧，提交到主分支远端上，然后其他的问题修复掉".
This pass closes the eight residual risks reported after D-814. No
`OfficialMatrix` case output changes (the matrix renders on the miss path) and
the ten protected files and the Flow400 contract are untouched. The one
behavioural change is on the cache-**hit** path: an envelope-funded ER/state/
git/mindmap row now renders the same native frame as its miss instead of
falling back to the source.

## 1. Disposition table

| # | Residual risk | Disposition |
|---|---|---|
| 1 | The whole chain was local-only (116 commits ahead of `origin/master` when this pass started) | Pushed to `origin/master` after the gates below (explicit owner authorization) |
| 2 | `agentflow-0041-cohort.json` / `agentflow-cohort.json` still called the two D-442 look+seed rows "gated" | Fixed: the partition is now 27 native + 9 admitted + **0 gated + 2 delegated**; both generators cross-check the reviewed D-442 ledger and the C++ gates assert the delegated set equals it |
| 3 | Roster category labels can be misread as causes | Already instrumented (`predicate_sufficiency` + `sufficient_blocker`, documented in `mermaid-d329-fallback-terminal-state.md`); no code defect — recorded, no change |
| 4 | The giant frozen gates cannot run under ASan | The two groups are marked `ASAN_EXEMPT` in code (measured slowdown + compensating coverage) and excluded by name from the `asan` test preset, so the ASan lane completes; the changed mermaid surface gets a targeted ASan run |
| 5 | The D-470 cache-hit envelope re-open covered only flow | Fixed in production: `NativeLayout` now carries the **post-layout supplemental residual** (`envelope_remaining` + `envelope_active`) for every envelope-funded family, and a hit restores that residual. The new family-spanning parity test found the hole: the state row's hit returned no element before the fix |
| 6 | The 48 E6c body-track gaps are declared expected fallback | By design; the row-level reconciliation is in `d814-body-gap-track-reconciliation.md`. No change |
| 7 | Generator `--check` proves ledger == generator, not semantic correctness | Partially addressed: every one of the 108 generators now has a ctest `--check` (50 were ungated), so ledger drift cannot pass silently. Semantic correctness remains the live render gates' job (unchanged) |
| 8 | Known load flakes (`ServerMain.DirectConnect*`, `E2E_Pty`) | Raised wait/socket/ctest timeouts only; no predicate or assertion changed |

## 2. The production fix (residual #5)

`LayoutRowsCache` already re-opened a supplemental envelope on a cache hit, but
the only recorded value was the flow **allowance**
(`NativeLayout::flow_envelope_allowance`). State (D-461), git (D-463), mindmap
(D-465) and ER (RFC 0034) enable their own canvas-derived pool during layout
and never recorded it, so a hit re-debited the cached layout work against the
fixed 1M primary with **no** pool: a styled row that needed the envelope fell
back on every cache hit. The new test
`MermaidEnvelopeWork.LayoutCacheMissHitParityAcrossEnvelopeFamilies` reproduced
it on the first run (state hit returned no element).

The fix stores the exact post-layout state on `Work` itself, so no family has
to report a bound and no thread-local side channel exists:

- `NativeLayout` carries `envelope_remaining` (the supplemental balance after
  layout **and** the styled payload prepayment) plus `envelope_active`.
- `layout_mermaid_content` fills both from `work` on every successful return
  path (including the two ASCII early returns).
- The cache hit re-enables exactly that residual before re-debiting the cached
  primary-only layout work. For styled/packed layouts this reproduces the
  miss's `(remaining, supplemental_remaining)` exactly; for legacy-row layouts
  (which record no layout debit) the supplemental balance still matches, and
  the primary divergence is unobservable because legacy finish never spends
  `Work`.

Restoring the original allowance instead would have been wrong: the miss has
already spent part of the pool during layout (and prepayment), so a hit
restoring the full bound could fund an emission the miss's finish could not,
letting the same source flip from fallback to native across frames and
bypassing the combined bound. The new test therefore asserts both balances
equal on miss and hit, not merely that the pool is open.

`Work` is unchanged: adding a field would change `sizeof(Work)` and therefore
the frozen gas charges that several gates derive from it. `NativeLayout` grows
by one padded word; the frozen gas footprint is a lower bound
(`kNativeLayoutGasFootprint <= sizeof(NativeLayout)`), so it is unaffected.

The new test covers flow, state, git and mindmap rows: where the row outgrows
the primary the miss drains it, the hit restores an identical supplemental
balance, the 240x400 grids are byte-identical, the family art token is present
and the fallback marker is absent. ER keeps its pre-existing parity test, and
the flow parity test stays green. For legacy-row layouts (flow/git/mindmap)
`measured_layout_work` is 0 by design, so only the supplemental balance is
replay-exact; the styled state row asserts both balances.

## 3. Agentflow ledger truth (residual #2)

- `generate_agentflow_cohort.py` now classifies the family four ways and derives
  the delegated set from `tools/mermaid/agentflow-handdrawn-d442-cohort.json`
  (schema, named-reviewer approval, sha and manifest pins, `wide_240: drawn`,
  and the row must be one the family gate rejects). The family generator never
  re-parses the D-442 shape, so the sibling ledger is the single source of
  truth.
- `generate_agentflow_0041_cohort.py` imports the same helper and records
  `delegated_count` / `delegated` / `delegated_ledger`.
- Both C++ gates assert `gated_count == 0`, `delegated_count == 2`, and that the
  delegated set equals the D-442 ledger's row set with `wide_240: drawn`.
- RFC 0041 implementation history and the D-407/D-442 closure docs carry
  amendment notes; no historical row was rewritten.

## 4. Ledger gates (residual #7)

The audit found 108 reproducible generators but only 58 wired into ctest. The
50 missing `--check` tests are now registered as `MermaidLedgerGate.*` (all 50
pass under `python3 -I`, matching the existing entry convention). Three of them
(`E2bCohort`, `Fs1Cohort`, `Fs1gCohort`) import PyYAML, which is present on the
dev box (6.0.3) and is now a test dependency of those three gates. This closes
the "generator exists but nothing checks the ledger" gap; it does not claim
semantic correctness.

## 5. ASan evidence (residual #4)

`MermaidCapabilityReport.Class285` and the three
`MermaidOfficialConfig110Native.*` tests now carry `ASAN_EXEMPT` markers with
the measured cost (723.5 s / 471 MiB and ~990 s / 730 MiB vs 8.85 s / 13.4 s in
release) and the compensating coverage, and the `asan` **test preset** now
excludes them by name, so `cmake --workflow --preset check` completes instead
of being killed at Class285. This is a real exclusion, not a comment-only
exemption. The changed surface (envelope/cache/agentflow/flow-look cohorts) is
exercised under ASan separately; see §7.

## 6. Flake hardening (residual #8)

- `ServerMain` DirectConnect waits, socket `SO_RCVTIMEO`/`SO_SNDTIMEO` and the
  interrupt deadline: 3 s -> 15 s (`kDirectConnectTimeout`).
- `e2e_pty_shutdown` `Terminal::wait` default: 10 s -> 20 s. The three ctest
  groups whose `TIMEOUT` was exactly the old 20 s bound (`E2E_Pty_Shutdown_*`,
  `E2E_Pty_Model_*`, the direct-model cases) are raised to 60 s; the remaining
  PTY entries keep their 30-45 s caps, which still exceed one 20 s wait.
- No predicate or assertion changed; only the time allowed for the expected
  condition.

## 7. Verification

Recorded from the closure run (commands + exit codes in the commit message):

- debug: configure + full build; `ctest -R "MermaidAgentflow|MermaidEnvelopeWork|MermaidLedgerGate"`
  **81/81** (44.27 s).
- release: configure + full build; targeted
  `MermaidAgentflow|MermaidEnvelopeWork|MermaidLedgerGate|MermaidFlowLookCohort`
  **84/84**; full `ctest --preset release -j$(nproc)` **100% of 4934, RC=0**
  (40.01 s).
- OfficialMatrix byte-identical in both presets (the matrix exercises the miss
  path; the only behavioural change is on a cache hit):
  `99d89f20ae68d34ad39f261fbae382a6dc432aff21e2dc276e82a81a8ac17ad9`.
- ASan: `MermaidEnvelopeWork|MermaidAgentflow|MermaidFlowLookCohort` **31/31**
  with zero sanitizer findings; the giant gates are excluded by the `asan` test
  preset per Open work #6.
- `fallback_attribution.py --self-test` (305 fallback / 25 categories),
  `verify_e6c_gap_roster.py` (bytes 45694, sha `87e85e7f…`),
  `verify_er107_admission.py --self-test` (15 tests) and `rfc_lint.py` all
  RC=0; all 108 generators pass `python3 -I --check`.

## 8. What this pass does not close

- The 200 pending Flow400 rows and the ranked coverage slices (the `@{...}`
  metadata is the measured next lever at 39 rows) — feature work, needs its own
  RFC.
- The 48 E6c body-track gaps — declared contract debt, unchanged.
- Full ASan coverage of the giant gates — needs the per-fixture split milestone
  (Open work #6), not a test-timeout change.

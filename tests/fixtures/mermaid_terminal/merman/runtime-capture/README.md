# Merman public authored-input runtime capture

This is an additive, maintenance-time test-data handoff from Merman `v0.9.1`,
commit `f6202f702dbde0a75049b1a62b3124feae709b3d` (MIT OR Apache-2.0).
It does not link an upstream renderer into Loom. The original 100-case corpus,
its handoff lock, and all frozen C1 vendor assets remain unchanged.

## Executed upstream scope

A complete pinned workspace is materialized in a scratch directory. Observers
record final arguments/returns at the public core typed-parse entrypoint and the
four public ASCII model/parsed render entrypoints. The complete command passes:

```
cargo test --locked --manifest-path <scratch>/Cargo.toml \
  -p merman-ascii --features all-diagrams --lib --tests -- --test-threads=4
```

Both the preliminary and exact-binding runs passed 21 suites: **1,376 passed,
0 failed, 0 ignored**. This proves the upstream extraction run, not Loom success.

The exact run records 3,151 public parse invocations and **1,632 exact UTF-8
source identities**, including all 100 existing fixture identities. It records
4,332 ASCII invocations, of which 519 consume source-free or modified typed
models and 170 cannot be attributed to one equivalent prior source. They are
kept separate; no Mermaid text is synthesized for a typed-only test.

The lexical inventory is fully accounted: all 52 formatting templates resolve
to actual complete arguments in the same test/helper; 482 static occurrences
are observed inputs. The other six literals are four structured expected-output
strings and two assertion-failure messages. They are explicitly classified as
non-inputs, rather than executed or silently discarded.

## Binding and output meaning

Parser and renderer observations correlate only within the same process/test
using the complete exact typed-model Debug bytes and their SHA256. Parsed
rendering also requires exact metadata bytes. Multiple prior equivalent source
identities remain ambiguous. `invocations.json` retains event order and all
options/configuration references for independent review.

There are 1,366 sources with successful terminal text whose String API omits
projection metadata; **these are not counted as diagrammatic drawing passes**.
Eleven have observed primary diagrammatic reports. Sixteen have structured-text
output: Gantt, GitGraph, Journey, Kanban, Mindmap, Packet, Timeline and TreeView
are explicitly structured-only families in the pinned capability catalog.
For String returns that catalog is family-level evidence, not a per-render
measurement. Other sources remain 213 parse-only/ambiguous, 21 foreign policy or
render errors, and 5 parse errors. These classes total 1,632; none are Loom
semantic pass results. Fallback/empty reports never count as drawings.

Exact outputs, original test-function byte slices, pinned parser compatibility
models, options, reports, and metadata are separate assets. Original conditional
assertions and foreign cancellation/resource/viewport/color policies require
native adaptation. Returning nonempty text or source fallback does not prove
shape, topology, endpoint-marker, grouping, order, or chart-value preservation.
Unsafe terminal control text and width policy differences need explicit product
policy; upstream permissive behavior must not be silently copied.

## Assets and integrity

`../runtime-cases.json` is the proposed additional native-readable case manifest.
`oracle-models/` stores 1,452 deduplicated compatibility JSON models;
`reference-outputs/` stores 2,382 deduplicated upstream reference strings.
`assertions/` contains exact original test-function slices, not backend code.
`candidate-resolution.json` accounts for all 540 lexical occurrences.
`typed-model-only.json` retains unbound and ambiguous invocations explicitly.
`provenance.json` records original pins, counts, source byte spans and hashes.

`maintenance/instrument_upstream.py` installs observers into a fresh pinned
scratch workspace; it takes that workspace as its only argument.
`maintenance/export_captured.py` takes the Loom repository root. It expects the
scratch directory and captured exact log filenames under
`build/mermaid-renderer-research-20261019/`, plus the existing frozen Merman
inline inventory. Before export, preserve the original ASCII `src/lib.rs` as
`merman-original-ascii-lib.rs` there, directly from the pinned Git object.
The caller must set `MERMAN_OBSERVER_PATH` to
`merman-executed-inputs-exact.jsonl`, save Cargo output to
`merman-observer-test-exact.log`, and check Cargo's exit status before exporting.
Python/Rust/upstream runtimes are maintenance tools only; native tests consume
saved JSON/text. The raw exact log remains scratch-only and is fingerprinted in
the provenance, so the compact handoff does not duplicate repeated config data.

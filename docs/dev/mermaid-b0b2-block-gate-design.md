# B0b batch 2 design — Block packed gate and bookkeeping Work (read-only)

Status: **DESIGN ONLY for Tech Lead D-035 shared-change gate. No production
change.** Produced 2026-11-04 against `b6b4deee`. Measurements came from
temporary test-facade probes and two fprintf points in the gates; all probes
were reverted and the tree rebuilt clean. Batch 1 (`40f2f322`) moved only
the three real Element fees to the Block meter; this batch covers the seven
sources still rejected at 1,000,000 layout Work.

## 1. Measured layer costs (Work units, 1M budget)

Block-only parser (`block_layout_succeeds`) vs the full layout seam
(`layout_succeeds`: classify probe + envelope + gate1 + gate2 + block)
vs the public render (layout + packed finish).

### B-group — three sources, rejected inside the layout pipeline

| source | block | full layout | overshoot |
|---|---:|---:|---:|
| look-neo-with-the-neo-theme | 956,819 | 1,116,027 | 116,027 |
| look-neo-with-the-neo-dark-theme | 956,546 | 1,115,317 | 115,317 |
| rows-with-different-heights | 878,425 | 1,001,150 | 1,150 |

The block→layout delta is 158,771–159,208. Instrumented decomposition
(look-neo-theme): **gate2 alone = 122,818**
(`512 + 6·19,855 styled_bytes + 4·794 source`); the residual ~36k is
classify probe + frontmatter envelope + gate1
(`32R+128 + 4B+2S`). different-heights: gate2 = 91,610. No Element fee is
reached (emission_debit 0).

### A-group — four shapes-inside-redux sources, rejected at finish bookkeeping

`layout` = 922,515–966,665 (admitted); ElementWork now pays the real node
fees (65,536–127,936, charged on the Block meter). The residual tail is the
**8B/256U bookkeeping kept on layout Work**:

| source | layout left | bookkeeping paid | last failed debit |
|---|---:|---:|---:|
| shapes-redux-color | 33,335 | 33,136 | 3,328 |
| shapes-redux-dark-color | 42,077 | 41,408 | 2,304 |
| shapes-redux-dark-theme | 77,485 | 75,456 | 3,328 |
| (shapes-redux-dark-color-theme measured same shape) | | | |

Short by one run's 256U at the tail; total finish Work bookkeeping
36,464–78,784. Batch 1 deliberately left this fully on Work because,
unlike ER, Block has no producer that pre-enumerated the runs.

## 2. What the two gates actually charge

- gate1 `valid_packed_rows`: `32R+128` (bounds check) + `4B+2S`
  (byte/cell rescan). Block cells are produced by `block_layout`, so this
  is a real second scan of the payload.
- gate2 `prepay_styled_payload`: `512 + 6·styled_payload_bytes + 4·source`,
  where `styled_payload_bytes` is the frozen-88-footprint estimate also
  used verbatim as the cache entry byte measure. The ×6/×4 factors are
  abstract prepayment coefficients, not direct allocations; the parser's
  real reserves were already paid inside `block_layout`.

Both gates are **packed-path validation/preparation**, semantically closer
to Element construction than to parsing geometry: they run only after the
diagram is laid out and only gate the packed serial model that feeds
`finish_mermaid_payload`.

## 3. Options

### Option A (recommended): one Block "payload preparation" meter

Scope the Block meter (D-038 Option A constant family) to cover **everything
after `block_layout`**: classify/envelope stays on layout Work; gate1,
gate2, the 8B/256U finish bookkeeping and the 320R+512+544U Element fees go
on the Block meter. One fresh bounded `Work` per render (carried from the
Block dispatch through `NativeLayout` into the finish, same ownership
pattern as batch 1's Element meter; recharged fresh on every cache hit).

- Admits all seven residual sources (worst measured need ≈ 159k gates + 79k
  bookkeeping + 128k elements ≈ 366k above the current split; bounded far
  below a canvas-derived ceiling).
- Cap is fully derived: canvas terms (R≤1024, U≤240·R, B≤240·R,
  S≤240·R) plus the `kSourceLimit=65536` source terms and the capacity
  growth already bounded in `styled_payload_bytes` via checked_product.
  Implementation must show the static_assert arithmetic; no tuned number.
- ER untouched: its gates stay on layout Work against the ErPaint
  prepayment (frozen 992,611 contract), so this is a Block-scoped metering
  boundary, not a redefinition of the packed gates for ER.
- Non-packed families (Class/State StyledRows, Flow, Sequence, Git,
  Mindmap, Charts, legacy rows) do not call the Block dispatch: zero change.
- Requires plumbing the meter through the gate call sites
  (`valid_packed_rows`/`prepay_styled_payload` gain an optional Block-meter
  sink parallel to the ER prepaid pointer; defaults null = current
  behavior). This is the main blast radius.

### Option B: reprice gate2 downward on layout Work

Investigate the ×6 styled-bytes coefficient and capacity-vs-size
overcharge for Block and lower it (the D-014 capacity probe showed
capacity-based savings exist but were insufficient for ER). Cheaper
plumbing, but: touches the shared estimate used as the **cache byte
measure** (can desynchronize cache eviction), changes ER/Class/Flow
charges unless carefully conditional, and does nothing for the A-group
256U tail — it would still need a second change. Not recommended as the
primary path.

### Option C: only move the A-group bookkeeping

Move 8B/256U to the Block meter for the four shapes sources; leave B-group
rejected until a later gate decision. Smallest step, but leaves three
sources red and guarantees a second shared-gate change immediately after;
the meter boundary ("Element only" vs "preparation") then moves twice.

## 4. Shared-change gate obligations (per D-035)

1. Owning module: `loom.ui.visual` mermaid packed path. Local closure does
   not exist: `valid_packed_rows`, `prepay_styled_payload` and
   `styled_payload_bytes` are shared with ER/Class/Flow; gate signature
   changes ripple to every packed caller.
2. Affected callers/contracts: ER dispatch (must remain byte-identical,
   prepaid semantics preserved), Block dispatch, LayoutRowsCache hit/miss
   (meter fresh per render, entry stores no meter), testing seam/F4.
3. Smallest viable change: optional Block `Work*` parameter on the two
   gates, null everywhere except the Block dispatch; no constant/coefficient
   edits; `kWorkLimit` stays 1,000,000.
4. Required verification: Class/State/Flow/ER Work byte-identical (Class
   baseline pins, ER107 factory 992,611, F4 119,488 unchanged); matrix row
   diff confined to the seven Block rows, no drawn→failure; debug+release;
   roster untouched (Block has no roster rows); ASan/UBSan; two
   independent reviews.
5. Rollback: any non-Block Work change, any drawn→failure or new error
   reverts the batch.

## 5. Acceptance target

- B-group 3 + A-group 4 (seven sources total) all accept at 1M layout
  Work with the Block meter bounded; Block58 consumer planes for all seven
  match their pinned terminal models. Block matrix drawn 51→58, errors
  7→0 at the renderer/consumer layers.
- spece already passes both layers after batch 1; its browser-faithful
  space:N feature gap is tracked solely by the independent batch 3 cohort,
  not by these gates. Renderer acceptance is never credit by itself (the
  batch 1 spece lesson).

## 6. Non-goals

No kWorkLimit/predicate/coefficient changes, no ER schedule change, no
cache byte-measure change, no paper/roster regeneration, no batch-3
space:N work. No implementation before shared-change-gate approval.

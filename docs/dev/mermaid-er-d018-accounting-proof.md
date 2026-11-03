# D-018 accounting proof — finish_mermaid_payload narrow credit (ER only)

Read-only proof required before the D-018 code change. No production code yet.
Scope: the active **ER** packed-payload path only (`ErPaint::finish` →
`detail::finish_mermaid_payload`). Block/Class/State/Flow/Requirement receive a
zero credit and stay byte-identical.

## 1. The three Work gates applied to one ER packed payload

For one ER original the same payload (rows R, UTF-8 bytes B, style cells S,
style runs U) is charged in:

1. `valid_packed_rows`: `32R + 128 + 4B + 2S`
   (`mermaid_impl.cpp` `detail::valid_packed_rows`).
2. `prepay_styled_payload`: `512 + 6*styled_payload_bytes + 4*source`
   (`mermaid_style_impl.cpp`).
3. `finish_mermaid_payload` preflight (`mermaid_impl.cpp`), decomposed here:
   - entry Element/row:  `R*(8*sizeof(ftxui::Element)+64) + 512 = 192R + 512`
     (measured `sizeof(ftxui::Element) == 16` on this libc++ build);
   - per line:           `8B_line + 128` per line  ⇒ `8B + 128R`;
   - per run:            `U*(8*sizeof(StyledRun) + 8*sizeof(ftxui::Element)+288)`
                         = `U*(256 + 128 + 288) = 672U`
                         (measured `sizeof(StyledRun) == 32`,
                         `SemanticCellStyle` is 4 bytes).

Gates 1–2 are covered by the D-015 credit (proven against
`ErPaint::finish`). D-018 adds a credit for only the two **duplicate
bookkeeping** terms in gate 3:

- **byte re-scan** `8B` — finish re-reads the same UTF-8 cells; ErPaint already
  prepaid these bytes at `48B+2B` (and gates 1–2 add further byte fees), so the
  text-run buffers finish grows (≤2B over SSO) stay fully covered after the
  credit (≥52B charged for the same bytes);
- **StyledRun enumeration** `256U` (`8*sizeof(StyledRun)` per run) — finish never
  constructs a `StyledRun` (those exist only on the StyledRows path in
  `mermaid_style_impl.cpp`, whose `8*sizeof(StyledRun)` fee is untouched); it
  only re-counts the runs ErPaint already enumerated at `704U`.

Retained gate-3 fees (real allocations only; the packed path builds a bare
`hbox`, no `size` Element):
`192R + 512 + 128R + (128+288)U = 320R + 416U + 512`.

## 2. The credited terms are the same payload ErPaint already paid

`ErPaint::finish` (`mermaid_relations_core_impl.cpp`) builds the identical
plane and already charges:

- `48*B + 4*cells + 8*cells` per emission for reading/writing the UTF-8 cell
  bytes — dominates the re-scan `8B`;
- `U*(512 + 8*sizeof(std::string)) = 704U` for enumerating and preparing the
  style runs — dominates the re-enumeration `256U`.

Therefore per payload:

```
credit_finish = 8B + 256U  ≤  (48B+2B) + 704U  (ErPaint.finish prepaid, same B,U)
```

and the combined credit across all three gates is still bounded by what
`ErPaint::finish` paid on that same call. The credit never exceeds the proven
prepaid amount (it is applied as `min(fee, prepaid)`), so it cannot turn any
rejection into an under-counted acceptance.

## 3. The retained fees still upper-bound real ftxui allocations

`finish_mermaid_payload` genuinely allocates an ftxui element tree (grep the
function):

- one `ftxui::hbox(... ) | ftxui::size(...)` Element plus its backing
  `ftxui::Elements` vector per row, then one `ftxui::vbox` and one
  `MermaidNode` shared_ptr;
- per run, at minimum one `ftxui::text(run.text)` Element, plus up to three
  decorator Elements (`color`, `bgcolor`, `bold`), pushed into that row's
  vector.

Bounds:

- row/tree level `320R + 512`: ≥ one hbox Element + vector capacity + amortized
  growth per row, plus vbox/node; conservative vs the actual ~one container and
  one vector node per row.
- run level `416U`: bounds one text Element (`8*16=128`) plus up to color/bgcolor
  decorator Elements (`192`; ER never sets the bold bit, so at most two), and
  node/vector growth slack (`96`); every run creates at least the text node,
  so `416U` never under-counts.

No allocation covered by these retained fees is removed; only double bookkeeping
is credited. An independent accounting review must confirm this section against
the ftxui allocation sites before code lands.

## 4. Why the credit is ER-only and invisible to other families

The credit out-param is set solely on the `er_paint` return path of
`relations_layout`; every other caller of `finish_mermaid_payload`
(Block) and `prepay_styled_payload`/`valid_packed_rows` (Class/Flow/State)
passes a null/zero credit, so their fees, consumed Work and rejection outcomes
are unchanged.

Cache propagation (required): a cache hit does not rerun `ErPaint::finish`, but
`LayoutRowsCache::Impl::resolve` re-debits `entry.layout_work` on a hit
(`mermaid_cache_impl.cpp:123`), whose measured value already includes every
ErPaint finish fee (`mermaid_impl.cpp` measured_layout_work). The credit must
therefore travel with the payload: stored as a field on `NativeLayout`,
persisted in `LayoutRowsCache::Impl::Entry` (mapped back to `NativeLayout` on a
hit), defaulting to zero for Block and every non-ER family. With that, the
uncached, miss and hit renders of an ER source are charged identically, and
Block/Class/State/Flow remain byte-identical.

## 5. Acceptance / hard stop

After D-015 + D-018 credits, all 107 ER107 originals must satisfy
consumed ≤ **992,611** with exact/one-unit-short invariants; if any of the nine
dense originals still exceeds 992,611 on retained (real) Element fees, this is
the last extension: stop, do not add a third credit class, and return to the
Tech Lead for a separate Element-emission budget decision.

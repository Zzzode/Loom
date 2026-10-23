# Requirement native witness scenes (proposed, independently authored)

These sources are test-first controls authored independently of the eleven
official fixtures.  They are complete Requirement documents, not fragments and
not expected terminal snapshots.  A future test must bind each literal's SHA,
then publish the actual five UTF-8 row arrays before decoding its Model:
`source_control_240`, `source_control_40`, `wide_240`, `narrow_40`, and
`restored_240`.  The full per-cell Grid captured directly from `ftxui::Screen::PixelAt` remains geometry authority, preserving dimensions, blank cells and wide-glyph continuation cells.  Row arrays
are retained for report/reproduction only and must never become hand-authored
coordinates or a substitute for ports, ownership, or ink checks.

Each scene uses the same Markdown node at 240 -> 40 -> 240 columns and a
following `AFTER_REQUIREMENT` sentinel.  If a wide diagram exceeds 40 columns,
the narrow state must be complete ordinary source, not a clipped diagram.  The
restored grid must equal the wide grid.  A mandatory positive fallback is a
failed scene and earns no official/terminal coverage credit.

## P1: all markers, kinds, reverse spelling and parallel edges

```mermaid
requirementDiagram
direction LR
requirement R {
  id: R-1
  text: owner requirement
  risk: high
  verifymethod: test
}
functionalRequirement F {
  id: F-1
  text: child requirement
  risk: low
  verifymethod: inspection
}
interfaceRequirement I {
  id: I-1
  text: endpoint requirement
  risk: medium
  verifymethod: analysis
}
performanceRequirement P {
  id: P-1
  text: performance requirement
  risk: medium
  verifymethod: demonstration
}
physicalRequirement H {
  id: H-1
  text: physical requirement
  risk: low
  verifymethod: test
}
designConstraint D {
  id: D-1
  text: constraint requirement
  risk: high
  verifymethod: inspection
}
element E {
  type: test system
  docref: docs/e
}
R - contains -> F
F - contains -> I
E - copies -> R
R - traces -> F
R - traces -> F
F - derives -> P
P - satisfies -> H
H - verifies -> D
D - refines -> R
I <- copies - E
```

Expected typed facts: six Requirement roles, one Element role, source order
`R,F,I,P,H,D,E`, ten statement-order edges, and two distinct parallel
`R -> F` `traces` records.  `I <- copies - E` resolves `E -> I`.  Both contains
edges have source `⊕`, solid route body and no target marker; all eight other
edges have a dashed body and one target-facing open-arrow.  The oracle must
observe every label `<<kind>>`, direction LR, all stereotype/name/metadata
rows, every relation's true source/target and its own physical route.

## P2: role collision and first-definition/first-field semantics

```mermaid
requirementDiagram
requirement Same {
  id: first-id
  id: discarded-id
  text: first text
  text: discarded text
  risk: low
  risk: high
  verifymethod: analysis
  verifymethod: test
}
requirement Same {
  id: duplicate-definition
  text: duplicate-definition
  risk: medium
  verifymethod: inspection
}
element Same {
  type: first element type
  docref: first/doc
}
element Same {
  type: discarded element type
  docref: discarded/doc
}
Same - satisfies -> Same
```

Expected typed facts: two visible nodes with different private geometry
identities but the same logical name.  The Requirement has `first-id`, `first
text`, `Low`, `Analysis`; the Element has `first element type`, `first/doc`.
The sole edge resolves Requirement -> Requirement because upstream resolution
checks the Requirement map first.  The Model mutation suite must reject a
collapsed one-node view, an Element-selected endpoint, the second duplicate
definition, and any second lexical field winning.

## P3: unresolved endpoints stay explicit and non-positive

```mermaid
requirementDiagram
requirement Known {
  id: known-id
  text: retained requirement
  risk: low
  verifymethod: test
}
Known - traces -> Missing
Ghost - verifies -> Known
```

Expected parse/typed result: one Requirement and two relationship statements
whose absent endpoints are marked unresolved.  Expected terminal result:
whole-source fallback with `unresolved_requirement_endpoint`, following source
and sentinel preserved, no fabricated `Missing`/`Ghost` box and no geometry
credit.  This is a source-semantics control, not an assertion that Mermaid's
browser renderer has a terminal-compatible failure mode.

## P4: greedy direction and quoted-name lexical boundary

```mermaid
requirementDiagram
prefix direction RL trailing text
requirement "req name with spaces" {
  id: quoted-id
  text: quoted name stays a name
  risk: medium
  verifymethod: demonstration
}
element "element name with spaces" {
  type: quoted type
  docref: docs/quoted
}
"element name with spaces" - satisfies -> "req name with spaces"
```

Expected typed facts: direction RL, two logical names without quote delimiters,
and Element -> Requirement.  This binds the actual greedy source direction
token and quote state.  The test must reject a Class-style identifier decoder,
literal quote display, an unrecognised greedy direction line, or a direction
derived from source-text search after the typed parse.

## Physical and semantic negative contract

For every positive P1/P2/P4 wide scene, mutate the decoded complete Model or
Grid after a good baseline.  The oracle must reject each of the following:

1. `⊕` replaced by `>`, `<`, `▷`, `◆`, `◇`, or an ordinary junction;
2. a circle-plus moved onto the source box perimeter, moved to the target, or
   given two incident route arms;
3. an open arrow replaced by a filled triangle, diamond/cardinality marker,
   moved to the source, or oriented away from its target;
4. a contains body made dashed, a non-contains body made solid, a relationship
   label changed, or a label intersected by route ink;
5. a swapped/reversed endpoint, dropped parallel edge, reordered relation,
   collapsed collision, omitted empty-role stereotype/name, omitted metadata
   row, or modified logical name versus metadata ID;
6. a route cell that is nonblank but is not claimed by a box, marker, edge body
   or label; and
7. missing following sentinel, narrow clipping, or a restored frame differing
   from the first 240-column frame.

The marker tests are physical: inspect the actual cell's one-cell glyph width,
perimeter adjacency, normal direction and route ownership.  They cannot pass by
searching for a text token anywhere in published rows.  Resource exhaustion,
parse failure, report/protocol failure and source fallback are reported in
separate categories; none is negative geometry evidence.

## P5: whole envelope ownership before body admission

Canonical complete positive (no source suffix substitution):

```mermaid
---
description: >-
  Source envelope witness.
  Metadata remains inert.
---
requirementDiagram
requirement R {}
element E {}
E - satisfies -> R
```

The binding retains the exact complete prefix, body offset, description raw
field and value `Source envelope witness. Metadata remains inert.`; config,
title and style are absent. The same complete body is used in each exact
negative partner: replace opening `---` with `----`; delete the closing fence;
replace `description: >-` with `description: |`; add root `unknown: yes` before
the closing fence; add root `config: {}`; add root `title: hidden`; duplicate
the `description: >-` root and its payload; replace the first two-space payload
indent with a tab; replace it with one space; insert a bare CR in the prefix.
Each is a complete source mutation, classified non-positive before body Model
admission. No fallback earns positive drawing or official coverage credit.
An inline-plain `description: Source envelope witness.` variant is a second
complete positive with its own full-source binding, not a partial-body test.

## P6: four actual target-facing open-arrow directions

Each of the following is a complete independent mandatory positive:

```mermaid
requirementDiagram
direction LR
element E {}
requirement R {}
E - satisfies -> R
```

```mermaid
requirementDiagram
direction RL
element E {}
requirement R {}
E - satisfies -> R
```

```mermaid
requirementDiagram
direction TB
element E {}
requirement R {}
E - satisfies -> R
```

```mermaid
requirementDiagram
direction BT
element E {}
requirement R {}
E - satisfies -> R
```

Each Model has Element E, Requirement R, one ordered dashed satisfies edge,
no metadata, and its literal direction. The actual Screen Grid must show the
node rank in that direction and the target exterior marker respectively `▷`,
`◁`, `▽`, `△`. The physical target port and final route step must agree;
merely finding a glyph elsewhere does not pass. Each glyph occupies exactly
one terminal cell. Rotating the marker or moving it onto the perimeter is a
good-first rejected physical mutant for each scene. No browser coordinate or
producer path/ownership metadata is consumed.

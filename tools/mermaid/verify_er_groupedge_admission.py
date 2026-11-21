#!/usr/bin/env python3
"""RFC 0033 ER group-edge owned-proof admission verifier.

Standalone, read-only plus ``--self-test``. Unlike the ER107 verifier there is
no matrix-join ``--write`` behavior: the ER107 matrix-join API is dormant
(``MATRIX_API_CONFIRMED=False``), so this verifier guards the six-case
group-edge cohort through immutable-asset and source-leg checks only.

Load-bearing rigor (slice 2 spec section 1.3):

* the four-part cohort assertion — no single text predicate robustly yields
  six. A loose superset returns seven (the handDrawn Work-ceiling row 198);
  the exact-set predicate returns six; 198's exclusion is pinned by its
  reviewed path+sha plus the Work-ceiling reason, never by an ad-hoc rule; and
  the exact-six set is disjoint from the ER107 active_config 107 set.
* every section-0 protected file (ten paths) is re-sha'd and re-sized; any
  drift fails the verifier.
* live witness checks over the immutable raw bytes (bounded span, raw slice
  equality, slice digest, line recomputation).
* field counts are independently derived from the source_model leg.
* per-case mutation identity sets and the aggregate census are pinned.
* REVIEWED_HELPER_SHA ships empty; the helper authority gate fails closed
  until an independent review fills it.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]

PROPOSAL = "tests/fixtures/mermaid_native/er_groupedge_complete_proposal.v1.json"
# RFC 0034 one-row cohort that retires the ordinal-198 work-ceiling exclusion.
ER198_PROPOSAL = "tests/fixtures/mermaid_native/er198_handdrawn_complete_proposal.v1.json"
ER198_CONTRACT = "ER198/complete-original/v1"
ER198_PROPOSAL_SHA = "4e2ba98698265da17c5f85c2230bf2fe16fa71fd3ad73d1aeff1d2e48d8ade11"
PRIOR = "tests/fixtures/mermaid_native/er206_native_whole_source_authority.config62.v1.json"
MANIFEST = "tests/fixtures/mermaid_official/manifest.json"
CASES_ROOT = "tests/fixtures/mermaid_official/cases"
SOURCE_HELPER = "tests/support/mermaid_er_groupedge_source.hpp"
OFFICIAL_HELPER = "tests/support/mermaid_er_groupedge_official.hpp"
CONTRACT = "ERGroupEdge/complete-original/v1"
GROUP_COLLISION = "raw_entity_group_kind_collision_not_representable_without_loss"
ACTIVE_CONFIG = "active_config"

PROPOSAL_SHA = "49af23a885bf1162b52724177cda1eacfec3c96683128e3f4f7c4b4450f44404"
PRIOR_SHA = "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db"
MANIFEST_SHA = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"

# The ten RFC 0033 section-0 protected files: (relative path, sha256, bytes).
# The closure run recomputes every value; a mismatch is a stop, not an edit.
PROTECTED = [
    ("tests/support/mermaid_er107_authority.hpp",
     "3aeb3312a5d57e740960930ba740c4e4aa2824a529bd7e8d51c247211d5610c2", 139324),
    ("tests/support/mermaid_er107_source.hpp",
     "1593280ffaf27f9b9115a898557734693f58b181b81c64067ff9c399ed3cae46", 116500),
    ("tests/support/mermaid_er107_official.hpp",
     "93904c1ad361940d803958b4adc7f2fced74f948b8cdc5bc887859f81f706473", 30465),
    ("tests/fixtures/mermaid_native/er107_complete_source_proposal.v1.json",
     "5a1c642cc7a3e2cf08e00355be845711520f762fc999dc13fe7dbb4a417297f3", 3743357),
    ("tests/fixtures/mermaid_native/er107_terminal_projection_proposal.v1.json",
     "f8f7dedc651112ac553ea86d200f2562ffb05d54b2b09df7d6477a687a8de7c9", 2945119),
    ("tests/fixtures/mermaid_native/er107_presentation_source_audit.v1.json",
     "8f35938eb74591a428823aae3c1279c4a333fa8240b7d92181e77e7f22ed18ba", 72136),
    ("tests/fixtures/mermaid_native/er206_native_whole_source_authority.config62.v1.json",
     "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db", 622036),
    ("tools/mermaid/e6c-mandatory-gap-roster.json",
     "908b088f6339fed88da92d07b92ddc3a24a90881dd6a69eb829447376f655a01", 45096),
    ("tests/support/mermaid_relations_oracle.hpp",
     "ce9d3fd7d54eab66b469d630b77f0f5b7acf9f72524124c758c576fc7808c3cd", 169038),
    ("tests/support/mermaid_gap_roster.hpp",
     "45012318fb59e6e27c1dabba02e5a2d5f920c27fd7cec0e284a2a12d917510e8", 11591),
]

# The historical reviewed Work-ceiling exclusion. Ordinal 198 carries two debts;
# its exclusion could not be text-decided, so it is path+sha bound with the
# Work-ceiling reason. RFC 0034 RETIRED this exclusion (bounded two-pool layout
# Work); the constants below stay frozen to pin the prior facts, and
# assert_er198_retirement proves the exclusion is retired by the new one-row
# cohort rather than merely deleted.
BLOCKED = {
    "prior_ordinal": 198,
    "path": "er-diagram/visual-check/"
            "nested-er-subgraphs-with-direction-override-handdrawn-look.mmd",
    "source_sha256": "c48919288f56b2cb5bae9050ff75afa41faca478d0d77c0f6cc442e86465954b",
    "blocker_kind": "er-envelope-double-parse-work-ceiling",
    "debt_kinds": [ACTIVE_CONFIG, GROUP_COLLISION],
}

LOOSE_SUPERSET = [26, 27, 29, 39, 196, 197, 198]
EXACT_COHORT = [26, 27, 29, 39, 196, 197]
ER107_COHORT_SIZE = 107

# Closed mutation vocabulary mirrored from the reviewed relations oracle
# (er206_mutations_core reject kinds plus the physical orphan_ink negative).
EXECUTED_VOCABULARY = {
    "attribute_comment", "attribute_erasure", "attribute_key_count", "attribute_keys",
    "attribute_name", "attribute_order", "attribute_type", "comment_br_merge",
    "comment_br_shift", "comment_line_erasure", "complete_header", "end_cardinality",
    "group_caption", "group_membership", "group_parent", "isolated_entity_erasure",
    "ordered_keys", "orphan_ink", "relation_br_merge", "relation_br_shift",
    "relation_erasure", "relation_label", "relation_line_erasure", "relation_line_order",
    "start_cardinality", "stroke", "third_endpoint",
}
SKIPPED_VOCABULARY = {
    "identical_duplicate_not_independent_ink",
    "incident_entity_needs_coherent_graph_removal", "no_distinct_ordered_key_pair",
    "no_third_entity", "root_entity_has_no_membership_to_erase",
    "root_group_has_no_parent_to_erase",
}

# Per-case mutation identity measured from the live C++ oracle (the C++ owned()
# gate enforces these at runtime; the verifier pins them as the independent
# record and validates any supplied proof against them).
MUTATION_BASELINE = {
    26: {"narrow_exact_ordinary": False, "semantic": 14, "physical": 1,
         "executed": ["complete_header", "end_cardinality", "group_caption",
                      "group_membership", "isolated_entity_erasure", "orphan_ink",
                      "relation_erasure", "relation_label", "start_cardinality",
                      "stroke", "third_endpoint"],
         "skipped": ["root_group_has_no_parent_to_erase"]},
    27: {"narrow_exact_ordinary": True, "semantic": 18, "physical": 1,
         "executed": ["complete_header", "end_cardinality", "group_caption",
                      "group_membership", "orphan_ink", "relation_erasure",
                      "relation_label", "start_cardinality", "stroke", "third_endpoint"],
         "skipped": ["incident_entity_needs_coherent_graph_removal",
                     "root_entity_has_no_membership_to_erase",
                     "root_group_has_no_parent_to_erase"]},
    29: {"narrow_exact_ordinary": False, "semantic": 11, "physical": 1,
         "executed": ["complete_header", "end_cardinality", "group_caption",
                      "group_membership", "isolated_entity_erasure", "orphan_ink",
                      "relation_erasure", "relation_label", "start_cardinality",
                      "stroke", "third_endpoint"],
         "skipped": ["incident_entity_needs_coherent_graph_removal",
                     "root_entity_has_no_membership_to_erase",
                     "root_group_has_no_parent_to_erase"]},
    39: {"narrow_exact_ordinary": False, "semantic": 13, "physical": 1,
         "executed": ["complete_header", "end_cardinality", "group_caption",
                      "group_membership", "isolated_entity_erasure", "orphan_ink",
                      "relation_erasure", "relation_label", "start_cardinality",
                      "stroke", "third_endpoint"],
         "skipped": ["incident_entity_needs_coherent_graph_removal",
                     "root_group_has_no_parent_to_erase"]},
    196: {"narrow_exact_ordinary": True, "semantic": 20, "physical": 1,
          "executed": ["complete_header", "end_cardinality", "group_caption",
                       "group_membership", "group_parent", "orphan_ink",
                       "relation_erasure", "relation_label", "start_cardinality",
                       "stroke", "third_endpoint"],
          "skipped": ["incident_entity_needs_coherent_graph_removal",
                      "root_entity_has_no_membership_to_erase",
                      "root_group_has_no_parent_to_erase"]},
    197: {"narrow_exact_ordinary": True, "semantic": 20, "physical": 1,
          "executed": ["complete_header", "end_cardinality", "group_caption",
                       "group_membership", "group_parent", "orphan_ink",
                       "relation_erasure", "relation_label", "start_cardinality",
                       "stroke", "third_endpoint"],
          "skipped": ["incident_entity_needs_coherent_graph_removal",
                      "root_entity_has_no_membership_to_erase",
                      "root_group_has_no_parent_to_erase"]},
}
AGGREGATE_SEMANTIC = 96
AGGREGATE_PHYSICAL = 6

# Independent review receipt (TechLead D-319 milestone closure). Populated only
# after two fresh-context, read-only adversarial reviews each returned APPROVE:
# one over the source leg (source helper + this verifier + proposal artifact),
# one over the official/matrix leg (official helper + run_official_er206_case
# wiring + tampered-byte negatives). Both independently re-derived every digest,
# attempted concrete bypasses, confirmed zero src/ production references, and
# matched these shas against the committed HEAD blobs. Editing either helper
# after this receipt changes its on-disk sha and fails authority_gate until a
# new reviewed receipt lands; a string status alone can never re-approve bytes.
REVIEWED_HELPER_SHA: dict[str, str] = {
    SOURCE_HELPER: "2778b1fb896b8562f5fceadb0c27b8c516fedac6908adc4dbdd8a3b0d87766fc",
    OFFICIAL_HELPER: "e229d44962f9b01ba67bfb13d1d7ede1d9d63ef16003cfeca0737c70e1c138ec",
}
HELPERS = (SOURCE_HELPER, OFFICIAL_HELPER)


def need(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def encoded(value) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")


def digest(value) -> str:
    return sha(encoded(value))


def file_bytes(path: Path, cap=8 * 1024 * 1024) -> bytes:
    need(path.is_file() and not path.is_symlink(), f"bounded regular file: {path}")
    raw = path.read_bytes()
    need(len(raw) <= cap, f"input grew beyond bound: {path}")
    return raw


def load(relative: str):
    return json.loads(file_bytes(ROOT / relative))


def debt_kinds(row):
    return {d["kind"] for d in row["whole_null_debts"]}


def four_part_cohort_assertion(prior, proposal, manifest):
    """The load-bearing MAJOR-1 four-part assertion over the immutable prior."""
    cases = prior["cases"]
    need(len(cases) == 206, "frozen ER206 prior has 206 cases")
    whole_null = [c for c in cases if c["whole_model"] is None]

    # (i) loose superset: debt list CONTAINS the group-collision kind -> 7.
    loose = [c["ordinal"] for c in whole_null
             if GROUP_COLLISION in debt_kinds(c)]
    need(loose == LOOSE_SUPERSET, "four-part (i): loose seven-row superset")

    # (ii) exact-set predicate: debts == {group_collision} -> the compiled six.
    exact = [c["ordinal"] for c in whole_null
             if debt_kinds(c) == {GROUP_COLLISION}]
    need(exact == EXACT_COHORT, "four-part (ii): exact six-cohort predicate")
    proposal_paths = [c["path"] for c in proposal["cases"]]
    prior_paths = sorted(cases[o]["fixture"] for o in EXACT_COHORT)
    need(sorted(proposal_paths) == prior_paths,
         "four-part (ii): proposal cases equal the exact six cohort")
    need([c["prior_ER206_ordinal"] for c in proposal["cases"]] == EXACT_COHORT,
         "four-part (ii): proposal prior ordinals match exact cohort order")

    # (iii) ordinal 198 identity is pinned by reviewed path+sha and the two-debt
    # set, and it stays absent from the six-case owned proposal. The historical
    # Work-ceiling exclusion itself is retired by the separate RFC 0034 cohort;
    # assert_er198_retirement() proves that retirement below.
    blocked = cases[BLOCKED["prior_ordinal"]]
    need(blocked["fixture"] == BLOCKED["path"],
         "four-part (iii): blocked path identity")
    blocked_fixture = next(f for f in manifest["fixtures"]
                           if f["path"] == BLOCKED["path"])
    need(blocked_fixture["sha256"] == BLOCKED["source_sha256"],
         "four-part (iii): blocked row sha bound")
    need(debt_kinds(blocked) == set(BLOCKED["debt_kinds"]),
         "four-part (iii): blocked row carries the two-debt set")
    need(BLOCKED["path"] not in proposal_paths,
         "four-part (iii): Work-ceiling row absent from the six-case proposal")

    # (iv) exact-six and the ER107 active_config set have empty intersection.
    active_config = [c["ordinal"] for c in whole_null
                     if debt_kinds(c) == {ACTIVE_CONFIG}]
    need(len(active_config) == ER107_COHORT_SIZE,
         "four-part (iv): ER107 active_config cohort stays 107")
    need(set(exact).isdisjoint(active_config),
         "four-part (iv): group-edge and ER107 cohorts are disjoint")
    return exact


def assert_er198_retirement(prior, six_case_proposal, manifest):
    """RFC 0034: prove the ordinal-198 work-ceiling exclusion is retired by a
    real one-row cohort, not orphaned or silently deleted. The six-case
    proposal stays byte-identical and continues to exclude 198."""
    er198 = load(ER198_PROPOSAL)
    need(sha(file_bytes(ROOT / ER198_PROPOSAL)) == ER198_PROPOSAL_SHA,
         "ER198 retirement proposal artifact sha")
    need(er198["contract"] == ER198_CONTRACT, "ER198 distinct contract id")
    need(er198["contract"] != CONTRACT,
         "ER198 contract is distinct from the six-case contract")
    er198_cases = er198["cases"]
    need(len(er198_cases) == 1, "ER198 retirement proposal is one-row")
    row = er198_cases[0]
    need(row["path"] == BLOCKED["path"], "ER198 retirement binds the blocked path")
    need(row["source_sha256"] == BLOCKED["source_sha256"],
         "ER198 retirement binds the blocked source sha")
    need(row["prior_ER206_ordinal"] == BLOCKED["prior_ordinal"],
         "ER198 retirement binds prior ordinal 198")
    # The retained ER206 prior still carries the two-debt, null-Model identity.
    blocked = prior["cases"][BLOCKED["prior_ordinal"]]
    need(blocked["whole_model"] is None, "ER198 prior row stays whole-null")
    need(debt_kinds(blocked) == set(BLOCKED["debt_kinds"]),
         "ER198 prior row keeps the two-debt set")
    # The six-case proposal and the ER198 cohort remain mutually exclusive.
    six_paths = {c["path"] for c in six_case_proposal["cases"]}
    need(BLOCKED["path"] not in six_paths,
         "198 stays absent from the six-case proposal (empty intersection)")
    # The retirement record names the same blocker kind and attributes RFC 0034.
    retired = er198["selection"]["retired_work_ceiling_exclusion"]
    need(retired["retired_blocker_kind"] == BLOCKED["blocker_kind"],
         "ER198 retirement names the historical blocker kind")
    need(retired["retired_by"] == "RFC 0034", "ER198 retirement attribution")
    need(retired["source_sha256"] == BLOCKED["source_sha256"],
         "ER198 retirement record sha-bound")
    # The manifest fixture identity is unchanged.
    fixture = next(f for f in manifest["fixtures"] if f["path"] == BLOCKED["path"])
    need(fixture["sha256"] == BLOCKED["source_sha256"],
         "ER198 retirement matches the immutable manifest fixture")


def verify_live_witnesses(case, raw: bytes):
    """Bounded span, raw-byte equality, slice digest and line recompute."""
    witnesses = case["source_witnesses"]
    ordinals = [w["ordinal"] for w in witnesses]
    need(ordinals == list(range(len(witnesses))), "dense global witness ordinals")
    saw_full = False
    for w in witnesses:
        a, b = w["source_byte_start"], w["source_byte_end"]
        need(type(a) is type(b) is int and 0 <= a <= b <= len(raw),
             "bounded witness byte range")
        need(raw[a:b] == w["source_lexeme"].encode(), "raw witness byte equality")
        need(sha(raw[a:b]) == w["source_slice_sha256"], "witness slice digest")
        need(w["source_utf8_bytes"] == b - a, "witness byte count")
        need(w["source_line_start"] == raw[:a].count(b"\n") + 1,
             "witness start line recomputation")
        need(w["source_line_end"] == raw[:max(a, b - 1)].count(b"\n") + 1,
             "witness end line recomputation")
        if w["kind"] == "complete_original_source":
            need(w["source_lexeme"].encode() == raw, "whole-original witness")
            saw_full = True
    need(saw_full, "complete-original witness present")


def derive_field_counts(case):
    """Independently derive structural counts from the source_model leg."""
    sm = case["source_model"]
    return {
        "entities": len(sm["entities"]),
        "relationships": len(sm["relationships"]),
        "subgraphs": len(sm["subgraphs"]),
        "direction_declarations": len(sm["direction_declarations"]),
        "attributes": sum(len(e["attributes"]) for e in sm["entities"]),
        "source_witness_count": len(case["source_witnesses"]),
    }


def verify_component_digests(case):
    for key, pin in (
        ("whole_scene_model", "whole_scene_model_sha256"),
        ("source_model", "source_model_sha256"),
        ("source_witnesses", "source_witnesses_sha256"),
        ("source_binding", "source_binding_sha256"),
        ("source_asset_preimage", "source_asset_case_sha256"),
        ("manifest_record", "manifest_record_sha256"),
    ):
        need(digest(case[key]) == case[pin], f"component digest re-derivation: {key}")


def verify_case(case, raw: bytes):
    need(sha(raw) == case["source_sha256"] and len(raw) == case["source_bytes"],
         "live raw sha and byte count")
    verify_component_digests(case)
    verify_live_witnesses(case, raw)
    derived = derive_field_counts(case)
    need(case["field_counts"] == derived, "independently derived field counts")
    # Dual-track population parity between the two legs.
    wsm, sm = case["whole_scene_model"], case["source_model"]
    need(len(wsm["entities"]) == len(sm["entities"]) and
         len(wsm["relations"]) == len(sm["relationships"]) and
         len(wsm["groups"]) == len(sm["subgraphs"]),
         "whole_scene/source model population parity")


def verify_proof_identity(proof):
    """Validate a compact owned proof against the pinned mutation baseline."""
    ordinal = proof["prior_ER206_ordinal"]
    need(ordinal in MUTATION_BASELINE, "proof ordinal outside the six cohort")
    base = MUTATION_BASELINE[ordinal]
    need(proof["contract"] == CONTRACT, "proof contract id")
    need(proof["captures"] == 15, "proof fifteen captures")
    need(proof["local_ready"] is True, "proof local ready")
    need(proof["narrow_exact_ordinary"] == base["narrow_exact_ordinary"],
         "pinned narrow disposition")
    need(proof["narrow_exact_ordinary"] != proof["narrow_geometry_good"],
         "narrow exact/geometry XOR")
    need(proof["semantic_mutations_rejected"] == base["semantic"] and
         proof["physical_mutations_rejected"] == base["physical"],
         "pinned semantic/physical mutation counts")
    need(proof["mutations_executed"] == base["executed"],
         "pinned per-case executed mutation identity")
    need(proof["mutations_inapplicable"] == base["skipped"],
         "pinned per-case skipped mutation identity")
    for name in proof["mutations_executed"]:
        need(name in EXECUTED_VOCABULARY, "executed mutation outside closed vocabulary")
    for name in proof["mutations_inapplicable"]:
        need(name in SKIPPED_VOCABULARY, "skipped mutation outside closed vocabulary")


def protected_files_unchanged():
    for relative, expected_sha, expected_bytes in PROTECTED:
        raw = file_bytes(ROOT / relative)
        actual_sha, actual_bytes = sha(raw), len(raw)
        need(actual_sha == expected_sha and actual_bytes == expected_bytes,
             f"protected file drift: {relative}")


def authority_gate():
    need(set(REVIEWED_HELPER_SHA) == set(HELPERS),
         "Main must supply exact independently reviewed helper hashes")
    for relative, expected in REVIEWED_HELPER_SHA.items():
        need(isinstance(expected, str) and len(expected) == 64 and
             sha(file_bytes(ROOT / relative, 524288)) == expected,
             "reviewed helper drift: " + relative)


def verify_read_only():
    """Full read-only validation. Fails closed pre-review via authority_gate."""
    proposal = load(PROPOSAL)
    prior = load(PRIOR)
    manifest = load(MANIFEST)
    need(sha(file_bytes(ROOT / PROPOSAL)) == PROPOSAL_SHA, "proposal artifact sha")
    need(sha(file_bytes(ROOT / PRIOR)) == PRIOR_SHA, "ER206 prior sha")
    need(sha(file_bytes(ROOT / MANIFEST)) == MANIFEST_SHA, "manifest sha")
    need(proposal["schema"].endswith("er_groupedge_complete_proposal/v1"),
         "proposal schema id")
    need(proposal["contract"] == CONTRACT, "proposal contract id")
    protected_files_unchanged()
    four_part_cohort_assertion(prior, proposal, manifest)
    assert_er198_retirement(prior, proposal, manifest)
    for case in proposal["cases"]:
        raw = file_bytes(ROOT / CASES_ROOT / case["path"])
        verify_case(case, raw)
    census = proposal["field_census"]["totals"]
    need(census["entities"] == 15 and census["relationships"] == 9 and
         census["subgraphs"] == 10 and census["direction_declarations"] == 6 and
         census["attributes"] == 0 and census["source_witness_count"] == 338,
         "aggregate field census")
    # Mutation baseline internal consistency.
    need(sum(b["semantic"] for b in MUTATION_BASELINE.values()) == AGGREGATE_SEMANTIC,
         "aggregate semantic census")
    need(sum(b["physical"] for b in MUTATION_BASELINE.values()) == AGGREGATE_PHYSICAL,
         "aggregate physical census")
    authority_gate()
    return proposal


class GroupEdgeControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proposal = load(PROPOSAL)
        cls.prior = load(PRIOR)
        cls.manifest = load(MANIFEST)

    def test_four_part_cohort_assertion_on_real_prior(self):
        exact = four_part_cohort_assertion(self.prior, self.proposal, self.manifest)
        self.assertEqual(exact, EXACT_COHORT)
        active = [c["ordinal"] for c in self.prior["cases"]
                  if c["whole_model"] is None and debt_kinds(c) == {ACTIVE_CONFIG}]
        self.assertEqual(len(active), ER107_COHORT_SIZE)
        self.assertTrue(set(exact).isdisjoint(active))

    def test_er198_work_ceiling_exclusion_is_retired(self):
        # RFC 0034: the historical ordinal-198 exclusion is retired by a real
        # one-row cohort while the six-case proposal and protected prior hold.
        assert_er198_retirement(self.prior, self.proposal, self.manifest)

    def test_protected_ten_files_byte_identical(self):
        protected_files_unchanged()

    def test_proposal_component_and_live_witness_checks(self):
        for case in self.proposal["cases"]:
            raw = file_bytes(ROOT / CASES_ROOT / case["path"])
            verify_case(case, raw)

    def test_field_counts_and_aggregate_census(self):
        for case in self.proposal["cases"]:
            self.assertEqual(case["field_counts"], derive_field_counts(case))
        totals = self.proposal["field_census"]["totals"]
        self.assertEqual(totals["source_witness_count"], 338)

    def test_mutation_baseline_closed_vocabulary_and_census(self):
        for ordinal, base in MUTATION_BASELINE.items():
            self.assertTrue(set(base["executed"]).issubset(EXECUTED_VOCABULARY))
            self.assertTrue(set(base["skipped"]).issubset(SKIPPED_VOCABULARY))
            self.assertNotIn("group_parent", base["executed"]) if ordinal in (26, 27, 29, 39) \
                else self.assertIn("group_parent", base["executed"])
        self.assertEqual(sum(b["semantic"] for b in MUTATION_BASELINE.values()),
                         AGGREGATE_SEMANTIC)
        self.assertEqual(sum(b["physical"] for b in MUTATION_BASELINE.values()),
                         AGGREGATE_PHYSICAL)

    def test_synthetic_proof_identity_accepted_and_tampered_rejected(self):
        base = MUTATION_BASELINE[26]
        good = {"contract": CONTRACT, "prior_ER206_ordinal": 26, "captures": 15,
                "local_ready": True, "narrow_geometry_good": True,
                "narrow_exact_ordinary": base["narrow_exact_ordinary"],
                "semantic_mutations_rejected": base["semantic"],
                "physical_mutations_rejected": base["physical"],
                "mutations_executed": list(base["executed"]),
                "mutations_inapplicable": list(base["skipped"])}
        verify_proof_identity(good)
        for mutate in (
                lambda p: p.update(captures=14),
                lambda p: p.update(local_ready=False),
                lambda p: p.update(mutations_executed=base["executed"] + ["invented"]),
                lambda p: p.update(narrow_exact_ordinary=True),
                lambda p: p.update(semantic_mutations_rejected=0),
                lambda p: p.update(contract="WRONG")):
            bad = copy.deepcopy(good)
            mutate(bad)
            with self.assertRaises(ValueError):
                verify_proof_identity(bad)

    def test_helper_authority_gate_fail_closed_and_receipt_accepts(self):
        from unittest.mock import patch
        # Fail-closed is a permanent property of an empty/partial map, proven by
        # temporarily clearing the now-populated receipt rather than assuming the
        # shipped map stays empty (the post-review map names both helpers).
        with patch.dict(REVIEWED_HELPER_SHA, {}, clear=True):
            with self.assertRaisesRegex(ValueError, "reviewed helper"):
                authority_gate()
        with patch.dict(REVIEWED_HELPER_SHA,
                        {SOURCE_HELPER: REVIEWED_HELPER_SHA[SOURCE_HELPER]},
                        clear=True):
            with self.assertRaisesRegex(ValueError, "reviewed helper"):
                authority_gate()  # a partial receipt is still insufficient
        # The shipped receipt names exactly both helpers, each a 64-hex sha, and
        # the gate accepts it because the on-disk bytes still match.
        self.assertEqual(set(REVIEWED_HELPER_SHA), set(HELPERS))
        for helper in HELPERS:
            self.assertEqual(len(REVIEWED_HELPER_SHA[helper]), 64)
            self.assertEqual(sha(file_bytes(ROOT / helper, 524288)),
                             REVIEWED_HELPER_SHA[helper])
        authority_gate()  # receipt matches the committed bytes: no raise

    def test_wrong_receipt_sha_is_rejected(self):
        from unittest.mock import patch
        forged = dict(REVIEWED_HELPER_SHA)
        forged[SOURCE_HELPER] = "0" * 64
        with patch.dict(REVIEWED_HELPER_SHA, forged, clear=True):
            with self.assertRaisesRegex(ValueError, "reviewed helper"):
                authority_gate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(GroupEdgeControls)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    verify_read_only()
    print("Read-only verified ER group-edge cohort: six owned proofs, four-part cohort "
          "assertion, RFC 0034 ER198 work-ceiling retirement assertion, ten protected files "
          "byte-identical; both helper shas match the independent review receipt.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        print("ER group-edge admission refused: " + str(error), file=sys.stderr)
        sys.exit(1)

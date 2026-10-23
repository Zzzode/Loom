#[test]
fn routing_determinism_summary_mentions_primary_canaries() {
    let summary = route_diagnostics::determinism_summary_for_pair_ids(&["M14", "M11"], 3).unwrap();

    assert!(summary.contains("M14"), "{summary}");
    assert!(summary.contains("M11"), "{summary}");
    assert!(summary.contains("route_signature_variants="), "{summary}");
    assert!(summary.contains("route_deterministic="), "{summary}");
}
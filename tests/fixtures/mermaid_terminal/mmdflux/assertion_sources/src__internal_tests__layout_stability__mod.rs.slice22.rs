#[test]
fn route_input_summary_does_not_claim_all_inputs_are_identical() {
    let pair = mutations::pair_by_id("M14").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();
    let summary = diagnostic.summary();

    assert!(summary.contains("route_input_diffs="), "{summary}");
    assert!(summary.contains("decision_input_changes="), "{summary}");
    assert!(
        !summary.contains("upstream_layout_is_identical"),
        "{summary}"
    );
}
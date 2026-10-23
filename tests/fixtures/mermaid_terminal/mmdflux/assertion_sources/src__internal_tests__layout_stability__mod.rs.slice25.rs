#[test]
fn route_decision_summary_mentions_histogram_classes() {
    let pair = mutations::pair_by_id("M14").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();
    let summary = diagnostic.summary();

    assert!(summary.contains("route_decisions="), "{summary}");
    assert!(summary.contains("decision_input_changes="), "{summary}");
    assert!(
        summary.contains("Port")
            || summary.contains("LabelLane")
            || summary.contains("BendTopology")
            || summary.contains("PathShape")
            || summary.contains("LengthOnly"),
        "{summary}"
    );
}
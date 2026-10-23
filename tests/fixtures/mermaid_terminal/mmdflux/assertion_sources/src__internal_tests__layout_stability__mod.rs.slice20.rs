#[test]
fn m11_label_lane_decomposition_is_not_applicable() {
    let pair = mutations::pair_by_id("M11").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();

    assert_eq!(
        diagnostic.label_lane_non_applicability_reason.as_deref(),
        Some("M11 has no LabelLane route decision edges")
    );
}
#[test]
fn route_output_magnitude_collects_m11_without_label_lane_class() {
    let pair = mutations::pair_by_id("M11").expect("M11 pair");
    let output =
        proportionality::route_output_magnitude_for_pair(pair).expect("M11 output magnitude");

    assert_eq!(output.summary.changed_path_count, 7);
    assert!(output.summary.port_face_change_count > 0);
    assert_eq!(
        output.decision_edge_count(route_diagnostics::RouteDecisionClass::LabelLane),
        0
    );
}
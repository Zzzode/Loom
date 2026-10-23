#[test]
fn route_output_magnitude_collects_m14_changed_paths_and_additive_classes() {
    let pair = mutations::pair_by_id("M14").expect("M14 pair");
    let output =
        proportionality::route_output_magnitude_for_pair(pair).expect("M14 output magnitude");

    assert_eq!(output.summary.changed_path_count, 15);
    assert_eq!(output.paths.len(), 15);
    assert!(output.summary.sum_abs_route_len_delta > 0.0);
    assert!(
        output.summary.sum_abs_route_len_delta >= output.summary.signed_route_len_delta_sum.abs()
    );
    assert_eq!(
        output.decision_edge_count(route_diagnostics::RouteDecisionClass::PathShape),
        15
    );
}
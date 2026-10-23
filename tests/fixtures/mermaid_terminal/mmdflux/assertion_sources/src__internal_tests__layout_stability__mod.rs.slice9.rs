#[test]
fn route_proportionality_guardrails_preserve_prior_diagnostics() {
    let reports = report::run_corpus(mutations::tier_a_pairs()).expect("tier a reports");

    for pair_id in ["M05", "M07", "S02", "S03"] {
        let report = reports
            .iter()
            .find(|report| report.pair_id == pair_id)
            .expect("guardrail report");
        assert_eq!(
            report.routed.changed_path_geometry_count(),
            0,
            "{pair_id} should remain a zero-path-churn guardrail"
        );
    }

    let m14 = route_diagnostics::run_pair_route_diagnostics(
        mutations::pair_by_id("M14").expect("M14 pair"),
    )
    .expect("M14 route diagnostics");
    assert_eq!(
        m14.route_decisions.primary_histogram()[&route_diagnostics::RouteDecisionClass::LabelLane],
        6
    );
    assert_eq!(
        m14.route_decisions
            .edge_ids_for_class(route_diagnostics::RouteDecisionClass::PathShape)
            .len(),
        15
    );
    assert_eq!(
        m14.path_shape_correlation
            .as_ref()
            .expect("M14 path-shape correlation")
            .overlap_count,
        6
    );

    let m11 = route_diagnostics::run_pair_route_diagnostics(
        mutations::pair_by_id("M11").expect("M11 pair"),
    )
    .expect("M11 route diagnostics");
    assert_eq!(
        m11.route_decisions
            .edge_ids_for_class(route_diagnostics::RouteDecisionClass::LabelLane)
            .len(),
        0
    );

    let proportionality =
        proportionality::run_route_proportionality_report().expect("route proportionality report");
    assert!(proportionality.summary_for("M14").is_some());
    assert!(proportionality.summary_for("M11").is_some());
}
#[test]
fn route_proportionality_report_uses_explicit_event_control_families() {
    assert_eq!(
        proportionality::node_plus_edge_control_pair_ids(),
        ["M02", "M03"]
    );
    assert_eq!(
        proportionality::pure_edge_control_pair_ids(),
        ["M04", "M19", "M20", "M21"]
    );

    let report =
        proportionality::run_route_proportionality_report().expect("route proportionality report");

    for id in ["M02", "M03", "M04", "M19", "M20", "M21", "M14", "M11"] {
        assert!(report.summary_for(id).is_some(), "{id} missing from report");
    }

    assert!(!proportionality::pure_edge_control_pair_ids().contains(&"M11"));
    assert!(!proportionality::node_plus_edge_control_pair_ids().contains(&"M04"));
}
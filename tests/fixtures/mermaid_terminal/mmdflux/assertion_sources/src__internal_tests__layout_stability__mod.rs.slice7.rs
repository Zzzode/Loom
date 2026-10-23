#[test]
fn route_proportionality_report_exposes_pure_edge_cluster_summary() {
    let report =
        proportionality::run_route_proportionality_report().expect("route proportionality report");
    let cluster = report
        .pure_edge_cluster
        .as_ref()
        .expect("pure-edge cluster");

    assert_eq!(cluster.control_pair_ids, vec!["M04", "M19", "M20", "M21"]);
    assert!(cluster.p75_min > 0.0);
    assert!(cluster.p75_max >= cluster.p75_min);
    assert!(cluster.spread_ratio.unwrap_or_default() >= 1.0);
}
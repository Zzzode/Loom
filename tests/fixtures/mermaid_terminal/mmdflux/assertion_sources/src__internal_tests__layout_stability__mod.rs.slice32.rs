#[test]
fn layout_metrics_ignore_direct_impact_nodes_and_use_coord_tolerance() {
    let pair = mutations::pair_by_id("S01").expect("S01 pair should exist");
    let rendered = render_surfaces::render_pair(pair).unwrap();
    let report = mmds_metrics::compare_layout_mmds(
        pair,
        &rendered.before.layout_mmds,
        &rendered.after.layout_mmds,
    );

    assert_eq!(report.coord_eps, 0.01);
    assert!(report.direct_impact_nodes.iter().any(|id| *id == "X"));
    assert!(!report.unchanged_node_displacements.contains_key("X"));
    assert!(report.unchanged_node_count > 0);
}
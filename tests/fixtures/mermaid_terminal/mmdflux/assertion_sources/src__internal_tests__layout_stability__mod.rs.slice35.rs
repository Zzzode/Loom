#[test]
fn routed_metrics_report_label_rect_for_label_mutation() {
    let pair = mutations::pair_by_id("M06").expect("M06 pair should exist");
    let rendered = render_surfaces::render_pair(pair).unwrap();
    let report = mmds_metrics::compare_routed_mmds(
        pair,
        &rendered.before.routed_mmds,
        &rendered.after.routed_mmds,
    );

    assert!(
        report
            .label_rect_changes
            .iter()
            .any(|change| change.edge_id == "e0")
    );
    assert!(
        report
            .path_metrics
            .iter()
            .any(|metric| metric.edge_id == "e0")
    );
}
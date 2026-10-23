#[test]
fn routed_metrics_keep_visible_endpoint_and_port_intent_separate() {
    let pair = mutations::pair_by_id("M10").expect("M10 pair should exist");
    let rendered = render_surfaces::render_pair(pair).unwrap();
    let report = mmds_metrics::compare_routed_mmds(
        pair,
        &rendered.before.routed_mmds,
        &rendered.after.routed_mmds,
    );

    assert!(
        report
            .endpoint_face_changes
            .iter()
            .all(|change| change.source == "path")
    );
    assert!(
        report
            .port_intent_changes
            .iter()
            .all(|change| change.source == "port")
    );
}
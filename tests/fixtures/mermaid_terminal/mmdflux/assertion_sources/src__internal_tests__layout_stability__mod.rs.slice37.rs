#[test]
fn routed_metrics_treat_id_rename_incident_edges_as_removed_added() {
    let pair = mutations::pair_by_id("M05-ID").expect("M05 ID canary should exist");
    let rendered = render_surfaces::render_pair(pair).unwrap();
    let report = mmds_metrics::compare_routed_mmds(
        pair,
        &rendered.before.routed_mmds,
        &rendered.after.routed_mmds,
    );

    assert!(!report.removed_edges.is_empty());
    assert!(!report.added_edges.is_empty());
    assert!(
        report
            .path_metrics
            .iter()
            .all(|metric| !metric.subject_mentions_node("Lint"))
    );
}
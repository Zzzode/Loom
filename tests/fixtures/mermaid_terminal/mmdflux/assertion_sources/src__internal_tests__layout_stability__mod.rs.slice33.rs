#[test]
fn layout_metrics_treat_id_edits_as_remove_add_not_rename() {
    let pair = mutations::pair_by_id("M05-ID").expect("M05 ID canary should exist");
    let rendered = render_surfaces::render_pair(pair).unwrap();
    let report = mmds_metrics::compare_layout_mmds(
        pair,
        &rendered.before.layout_mmds,
        &rendered.after.layout_mmds,
    );

    assert!(report.removed_nodes.contains("Lint"));
    assert!(report.added_nodes.contains("Audit"));
    assert!(report.inferred_renames.is_empty());
}
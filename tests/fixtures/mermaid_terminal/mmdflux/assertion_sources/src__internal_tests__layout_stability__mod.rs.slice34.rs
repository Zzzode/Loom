#[test]
fn layout_metrics_do_not_collapse_global_shift_when_anchor_set_is_too_small() {
    let pair = mutations::pair_by_id("S01").expect("S01 pair should exist");
    let rendered = render_surfaces::render_pair(pair).unwrap();
    let report = mmds_metrics::compare_layout_mmds(
        pair,
        &rendered.before.layout_mmds,
        &rendered.after.layout_mmds,
    );

    if report.global_shift.anchor_count < 2 {
        assert_eq!(
            report.global_shift.confidence,
            mmds_metrics::ShiftConfidence::Low
        );
        assert!(!report.global_shift.collapsed_movement);
    }
}
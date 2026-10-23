#[test]
fn calibration_guardrails_keep_zero_path_churn_where_expected() {
    for id in ["M05", "M07", "S02", "S03"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();

        assert_eq!(
            report.routed.changed_path_geometry_count(),
            0,
            "{id}: {}",
            report.summary()
        );
    }
}
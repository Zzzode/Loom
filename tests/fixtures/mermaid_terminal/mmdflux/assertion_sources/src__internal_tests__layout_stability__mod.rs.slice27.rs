#[test]
fn secondary_localized_canaries_keep_layout_amplified_classification() {
    for id in ["M02", "M03", "M04"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();

        assert!(
            !report
                .phase_attribution()
                .route
                .layout_amplified_paths
                .is_empty(),
            "{id}: {}",
            report.summary()
        );
    }
}
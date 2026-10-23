#[test]
fn primary_phase_attribution_canaries_are_decomposed() {
    for id in ["M14", "M11", "M02", "M03", "M04", "M06"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();
        let attribution = report.phase_attribution();

        assert!(
            attribution.first_divergence().is_some(),
            "{id} should have a first divergence: {}",
            report.summary()
        );
        assert!(
            !attribution.phase_components_for_test().is_empty(),
            "{id} should report phase components: {}",
            report.summary()
        );
    }
}
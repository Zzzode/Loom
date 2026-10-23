#[test]
fn plan_0164_guardrails_preserve_0162_and_0163_calibration() {
    for id in ["M14", "M11"] {
        let pair = mutations::pair_by_id(id).unwrap();
        let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();
        assert_eq!(
            diagnostic.phase_attribution.first_divergence(),
            Some(PhaseName::Route)
        );
    }

    for id in ["M02", "M03", "M04"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();
        assert!(
            !report
                .phase_attribution()
                .route
                .layout_amplified_paths
                .is_empty()
        );
    }

    for id in ["M05", "M07", "S02", "S03"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();
        assert_eq!(report.routed.changed_path_geometry_count(), 0);
    }
}
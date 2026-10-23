#[test]
fn primary_route_canaries_keep_route_first_attribution_with_route_diagnostics() {
    for id in ["M14", "M11"] {
        let pair = mutations::pair_by_id(id).unwrap();
        let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();

        assert_eq!(
            diagnostic.phase_attribution.first_divergence(),
            Some(PhaseName::Route),
            "{id}: {}",
            diagnostic.summary()
        );
        assert!(
            diagnostic
                .phase_attribution
                .route
                .layout_amplified_paths
                .is_empty(),
            "{id}: {}",
            diagnostic.summary()
        );
    }
}
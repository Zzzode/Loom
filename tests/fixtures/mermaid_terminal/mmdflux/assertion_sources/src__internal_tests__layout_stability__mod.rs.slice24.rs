#[test]
fn m14_and_m11_route_decision_histograms_cover_changed_paths() {
    for id in ["M14", "M11"] {
        let pair = mutations::pair_by_id(id).unwrap();
        let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();

        let changed = diagnostic.phase_attribution.route.changed_paths.len();
        let histogram_total = diagnostic.route_decisions.primary_histogram_total();

        assert_eq!(histogram_total, changed, "{id}: {diagnostic:?}");
        assert!(changed > 0, "{id}: {diagnostic:?}");
    }
}
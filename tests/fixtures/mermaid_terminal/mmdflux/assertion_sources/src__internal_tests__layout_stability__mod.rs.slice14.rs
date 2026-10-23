#[test]
fn primary_canary_determinism_diagnostic_runs_requested_iterations() {
    for id in ["M14", "M11"] {
        let pair = mutations::pair_by_id(id).unwrap();
        let report = route_diagnostics::measure_pair_determinism(pair, 10).unwrap();

        assert_eq!(report.run_count, 10, "{id}: {report:?}");
        assert!(report.unique_signature_count >= 1, "{id}: {report:?}");
    }
}
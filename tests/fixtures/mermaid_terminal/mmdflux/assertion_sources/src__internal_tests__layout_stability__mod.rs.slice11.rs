#[test]
fn m14_and_m11_reports_include_route_attribution() {
    for id in ["M14", "M11"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();
        let summary = report.summary();

        assert!(summary.contains("first_divergence="), "{id}: {summary}");
        assert!(summary.contains("Route"), "{id}: {summary}");
    }
}
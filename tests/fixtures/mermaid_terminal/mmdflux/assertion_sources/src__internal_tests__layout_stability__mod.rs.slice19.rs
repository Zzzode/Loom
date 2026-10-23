#[test]
fn m11_route_input_diff_reports_added_route_edge_context() {
    let pair = mutations::pair_by_id("M11").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();

    assert!(
        !diagnostic.route_input_diff.added_edges.is_empty(),
        "{diagnostic:?}"
    );
    assert_eq!(
        diagnostic.phase_attribution.first_divergence(),
        Some(PhaseName::Route)
    );
}
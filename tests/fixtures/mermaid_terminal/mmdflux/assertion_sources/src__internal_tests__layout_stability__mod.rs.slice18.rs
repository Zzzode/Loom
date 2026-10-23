#[test]
fn m14_route_input_diff_reports_label_input_change() {
    let pair = mutations::pair_by_id("M14").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();

    assert!(
        !diagnostic.route_input_diff.changed_labels.is_empty(),
        "{diagnostic:?}"
    );
    assert_eq!(
        diagnostic.phase_attribution.first_divergence(),
        Some(PhaseName::Route)
    );
}
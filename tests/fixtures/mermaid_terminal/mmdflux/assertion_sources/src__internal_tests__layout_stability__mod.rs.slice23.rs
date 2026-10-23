#[test]
fn m14_path_shape_correlation_reports_counts_and_outcome() {
    let pair = mutations::pair_by_id("M14").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();
    let correlation = diagnostic
        .path_shape_correlation
        .as_ref()
        .expect("M14 path-shape correlation");

    assert_eq!(correlation.label_geometry_count, 6, "{correlation:?}");
    assert_eq!(correlation.path_shape_count, 15, "{correlation:?}");
    assert!(correlation.overlap_count <= 6, "{correlation:?}");
    assert_eq!(
        correlation.outcome,
        route_diagnostics::PathShapeCorrelationOutcome::MostlyOverlapsLabelGeometry,
        "{correlation:?}"
    );
}
#[test]
fn m14_label_lane_decomposition_reports_geometry_only_changes() {
    let pair = mutations::pair_by_id("M14").unwrap();
    let diagnostic = route_diagnostics::run_pair_route_diagnostics(pair).unwrap();
    let decomposition = diagnostic
        .label_lane_decomposition
        .as_ref()
        .expect("M14 label-lane decomposition");

    assert_eq!(decomposition.total_edges(), 6, "{decomposition:?}");
    assert!(
        decomposition
            .edge_deltas
            .keys()
            .all(|edge_id| edge_id.starts_with('e')),
        "{decomposition:?}"
    );
    assert_eq!(decomposition.histogram_total(), 6, "{decomposition:?}");
    assert_eq!(
        decomposition.histogram()[&route_diagnostics::LabelLaneDeltaClass::LabelGeometryOnly],
        6,
        "{decomposition:?}"
    );
    assert_eq!(decomposition.structural_total(), 0, "{decomposition:?}");
}
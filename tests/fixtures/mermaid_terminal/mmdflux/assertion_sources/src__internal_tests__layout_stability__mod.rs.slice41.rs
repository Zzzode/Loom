#[test]
fn m14_label_cascade_reports_route_and_port_contributors() {
    let report = report::run_pair(mutations::pair_by_id("M14").unwrap()).unwrap();
    let explanation = report.churn_explanation();

    assert!(report.routed.changed_path_geometry_count() > 0);
    assert!(report.quality.route_length_total_delta.abs() > 1.0);
    assert_ne!(report.quality.bend_count_total_delta, 0);
    assert!(!report.routed.path_port_divergence.is_empty());
    assert!(
        explanation
            .components
            .contains(&report::ChurnComponent::RouteGeometry)
    );
    assert!(
        explanation
            .components
            .contains(&report::ChurnComponent::PortDivergence)
    );
    assert!(
        explanation
            .components
            .contains(&report::ChurnComponent::LabelRect)
    );
}
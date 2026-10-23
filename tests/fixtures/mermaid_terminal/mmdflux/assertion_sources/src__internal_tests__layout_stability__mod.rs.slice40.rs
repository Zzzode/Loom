#[test]
fn m05_label_only_churn_is_attributed_to_label_and_bounds_not_route_geometry() {
    let report = report::run_pair(mutations::pair_by_id("M05").unwrap()).unwrap();
    let explanation = report.churn_explanation();

    assert_eq!(report.routed.path_metrics.len(), 6);
    assert_eq!(report.routed.changed_path_geometry_count(), 0);
    assert!(
        explanation
            .components
            .contains(&report::ChurnComponent::LabelRect)
    );
    assert!(
        explanation
            .components
            .contains(&report::ChurnComponent::Bounds)
    );
    assert!(
        !explanation
            .components
            .contains(&report::ChurnComponent::RouteGeometry)
    );
}
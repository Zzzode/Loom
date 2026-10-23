#[test]
fn style_only_canaries_explain_style_without_route_geometry_churn() {
    for id in ["M07", "S03"] {
        let report = report::run_pair(mutations::pair_by_id(id).unwrap()).unwrap();
        let explanation = report.churn_explanation();

        assert!(
            explanation
                .components
                .contains(&report::ChurnComponent::Style)
        );
        assert!(
            !explanation
                .components
                .contains(&report::ChurnComponent::RouteGeometry)
        );
        assert_eq!(report.routed.changed_path_geometry_count(), 0);
    }
}
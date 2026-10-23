#[test]
fn mixed_churn_exposes_all_detected_components() {
    let report = report::run_pair(mutations::pair_by_id("M14").unwrap()).unwrap();
    let explanation = report.churn_explanation();

    assert_eq!(report.render_churn, render_metrics::RenderChurnClass::Mixed);
    assert!(explanation.components.len() > 1);
    assert_eq!(
        explanation.components,
        explanation.components_sorted_for_test()
    );
}
#[test]
fn calibration_guardrails_stay_calibrated_with_phase_attribution() {
    let m05 = report::run_pair(mutations::pair_by_id("M05").unwrap()).unwrap();
    let m07 = report::run_pair(mutations::pair_by_id("M07").unwrap()).unwrap();
    let s02 = report::run_pair(mutations::pair_by_id("S02").unwrap()).unwrap();
    let s03 = report::run_pair(mutations::pair_by_id("S03").unwrap()).unwrap();

    assert_eq!(m05.routed.changed_path_geometry_count(), 0);
    assert!(
        m05.churn_explanation()
            .is_label_only_without_route_geometry()
    );
    assert!(
        m07.churn_explanation()
            .is_style_only_without_route_geometry()
    );
    assert_eq!(s02.routed.changed_path_geometry_count(), 0);
    assert!(
        s03.churn_explanation()
            .is_style_only_without_route_geometry()
    );
}
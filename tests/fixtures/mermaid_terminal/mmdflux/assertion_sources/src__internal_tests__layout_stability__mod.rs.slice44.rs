#[test]
fn h1_h2_canaries_have_calibrated_explanations() {
    let m07 = report::run_pair(mutations::pair_by_id("M07").unwrap()).unwrap();
    let s03 = report::run_pair(mutations::pair_by_id("S03").unwrap()).unwrap();
    let m05 = report::run_pair(mutations::pair_by_id("M05").unwrap()).unwrap();
    let m14 = report::run_pair(mutations::pair_by_id("M14").unwrap()).unwrap();

    assert_eq!(m07.routed.changed_path_geometry_count(), 0);
    assert_eq!(s03.routed.changed_path_geometry_count(), 0);
    assert_eq!(m05.routed.changed_path_geometry_count(), 0);
    assert!(m14.routed.changed_path_geometry_count() > 0);

    assert!(
        m07.churn_explanation()
            .is_style_only_without_route_geometry()
    );
    assert!(
        s03.churn_explanation()
            .is_style_only_without_route_geometry()
    );
    assert!(
        m05.churn_explanation()
            .is_label_only_without_route_geometry()
    );
    assert!(m14.churn_explanation().has_route_geometry_cascade());
}
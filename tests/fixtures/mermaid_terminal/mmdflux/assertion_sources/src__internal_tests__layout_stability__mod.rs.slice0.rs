#[test]
fn input_magnitude_collects_m14_label_dimension_without_edge_context() {
    let pair = mutations::pair_by_id("M14").expect("M14 pair");
    let magnitude = proportionality::input_magnitude_for_pair(pair).expect("M14 input magnitude");
    let label = magnitude
        .label_dimension
        .expect("label dimension magnitude");

    assert_eq!(label.changed_labels, 1);
    assert!(label.max_axis_delta > 0.0);
    assert!(label.total_axis_delta >= label.max_axis_delta);
    assert!(magnitude.edge_insertion.is_none());
    assert_eq!(
        proportionality::InputMagnitudeBasis::for_pair_id("M14"),
        proportionality::InputMagnitudeBasis::LabelDimensionUnits
    );
}
#[test]
fn input_magnitude_collects_m11_edge_insertion_context_without_label_units() {
    let pair = mutations::pair_by_id("M11").expect("M11 pair");
    let magnitude = proportionality::input_magnitude_for_pair(pair).expect("M11 input magnitude");
    let edge = magnitude.edge_insertion.expect("edge insertion magnitude");

    assert_eq!(edge.semantic_added_edge_ids, vec!["e7".to_string()]);
    assert!(edge.route_input_added_edges > 0);
    assert!(magnitude.label_dimension.is_none());
    assert_eq!(
        proportionality::InputMagnitudeBasis::for_pair_id("M11"),
        proportionality::InputMagnitudeBasis::EdgeInsertionContext
    );
}
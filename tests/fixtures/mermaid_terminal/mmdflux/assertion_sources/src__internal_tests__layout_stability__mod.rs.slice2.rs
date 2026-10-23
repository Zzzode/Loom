#[test]
fn input_magnitude_collects_control_axes_without_classifying_them() {
    for pair_id in ["M02", "M03", "M04"] {
        let pair = mutations::pair_by_id(pair_id).expect("control pair");
        let magnitude =
            proportionality::input_magnitude_for_pair(pair).expect("control input magnitude");
        assert!(
            magnitude.edge_insertion.is_some() || magnitude.node_insertion.is_some(),
            "{pair_id} should expose edge or node insertion context"
        );
    }

    for pair_id in ["M05", "M07", "S02", "S03"] {
        let pair = mutations::pair_by_id(pair_id).expect("guardrail pair");
        let magnitude =
            proportionality::input_magnitude_for_pair(pair).expect("guardrail input magnitude");
        assert!(
            magnitude.non_zero_axes().len() <= 1,
            "{pair_id} should stay simple enough to be a guardrail"
        );
    }
}
#[test]
fn pure_edge_input_magnitude_controls_collect_edge_context_without_other_axes() {
    for id in proportionality::pure_edge_control_pair_ids() {
        let pair = mutations::pair_by_id(id).expect("pure-edge control pair");
        let input =
            proportionality::input_magnitude_for_pair(pair).expect("pure-edge input magnitude");
        let edge = input.edge_insertion.expect("edge insertion magnitude");

        assert_eq!(
            edge.semantic_added_edge_ids,
            pair.direct_edges
                .iter()
                .map(|edge| edge.to_string())
                .collect::<Vec<_>>()
        );
        assert!(
            edge.route_input_added_edges > 0,
            "{id} should add route-input edge context"
        );
        assert!(
            input.label_dimension.is_none(),
            "{id} should not be a label mutation"
        );
        assert!(input.node_insertion.is_none(), "{id} should not add nodes");
        assert!(
            input.subgraph_change.is_none(),
            "{id} should not change subgraph semantics"
        );

        let output = proportionality::route_output_magnitude_for_pair(pair)
            .expect("pure-edge output magnitude");
        assert!(
            output.summary.changed_path_count > 0,
            "{id} should produce observable routed output magnitude"
        );
    }
}
#[test]
    fn subgraph_direction_lr_text_edges_remain_connected_after_external_centering() {
        let diagram = flowchart_fixture_diagram("subgraph_direction_lr.mmd");
        let layout = compute_layout(&diagram, &GridLayoutConfig::default());

        for (from, to) in [("Start", "A"), ("C", "End")] {
            let edge = diagram
                .edges
                .iter()
                .find(|edge| edge.from == from && edge.to == to)
                .unwrap_or_else(|| panic!("{from} -> {to} edge should exist"));
            assert!(
                !layout.edge_waypoints.contains_key(&edge.index),
                "{from} -> {to} should not keep stale edge waypoints after external centering"
            );
            assert!(
                !layout.routed_edge_paths.contains_key(&edge.index),
                "{from} -> {to} should not keep a stale routed draw path after external centering"
            );
            assert!(
                !layout.preserve_routed_path_topology.contains(&edge.index),
                "{from} -> {to} should not preserve stale route topology after external centering"
            );
        }

        let output = render_flowchart_fixture("subgraph_direction_lr.mmd");
        assert!(
            output.contains("│ Start │")
                && output.contains("Horizontal Flow")
                && output.contains("│ End │")
                && output.matches('▼').count() >= 2,
            "shifted external nodes should still render connected incoming and outgoing arrows. Output:\n{output}"
        );
    }
fn render_flowchart_supports_invisible_constraints_and_cross_markers() {
        let mut invisible = empty_flowchart();
        invisible.nodes = vec![node("A"), node("B")];
        invisible.edges = vec![FlowEdge {
            stroke: Some("invisible".to_string()),
            visibility: FlowEdgeVisibility::Invisible,
            ..edge("A", "B")
        }];

        let invisible_rendered =
            render_flowchart(&invisible, &AsciiRenderOptions::ascii()).unwrap();
        assert!(
            invisible_rendered.contains("| A |") && invisible_rendered.contains("| B |"),
            "invisible constraints should retain both positioned nodes:\n{invisible_rendered}"
        );
        assert!(
            !invisible_rendered.contains("| A |-") && !invisible_rendered.contains(">| B |"),
            "invisible constraints must not paint a terminal edge:\n{invisible_rendered}"
        );

        let mut cross = empty_flowchart();
        cross.nodes = vec![node("A"), node("B")];
        cross.edges = vec![FlowEdge {
            edge_type: Some("arrow_cross".to_string()),
            end_marker: FlowEdgeMarker::Cross,
            ..edge("A", "B")
        }];

        let cross_rendered = render_flowchart(&cross, &AsciiRenderOptions::ascii()).unwrap();
        assert!(
            cross_rendered.contains('x'),
            "cross marker should remain visible:\n{cross_rendered}"
        );
    }
fn flowchart_parser_top_down_skip_edge_routes_for_every_declaration_order() {
    // Dagre ranking owns topology before terminal routing. Equivalent edge declaration orders must
    // therefore keep the semantic A -> B -> C ranks even though route paint order can still vary.
    for input in [
        "flowchart TD\n  A --> B\n  B --> C\n  A --> C\n",
        "flowchart TD\n  A --> B\n  A --> C\n  B --> C\n",
        "flowchart TD\n  B --> C\n  A --> B\n  A --> C\n",
        "flowchart TD\n  B --> C\n  A --> C\n  A --> B\n",
        "flowchart TD\n  A --> C\n  A --> B\n  B --> C\n",
        "flowchart TD\n  A --> C\n  B --> C\n  A --> B\n",
    ] {
        let rendered = render_flowchart(input, &AsciiRenderOptions::ascii())
            .unwrap_or_else(|err| panic!("top-down skip edge should route for {input:?}: {err}"));

        for node in ["A", "B", "C"] {
            assert!(
                rendered.contains(node),
                "node {node} should stay visible for {input:?}:\n{rendered}"
            );
        }
        let a_line = first_line_index_containing(&rendered, "| A |");
        let b_line = first_line_index_containing(&rendered, "| B |");
        let c_line = first_line_index_containing(&rendered, "| C |");
        assert!(
            a_line < b_line && b_line < c_line,
            "Dagre ranks must stay invariant for {input:?}:\n{rendered}"
        );
        assert_rectangular_char_grid(&rendered);
    }
}
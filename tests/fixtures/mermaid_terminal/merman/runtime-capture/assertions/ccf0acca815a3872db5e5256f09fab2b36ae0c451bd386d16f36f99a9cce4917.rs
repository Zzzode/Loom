fn flowchart_parser_ranks_explicitly_declared_reverse_order_chain_by_connectivity() {
    let rendered = render_flowchart(
        "flowchart TD\n  C\n  B\n  A\n  A --> B\n  B --> C\n",
        &AsciiRenderOptions::ascii(),
    )
    .expect("reverse-declared chain should render through Dagre rank semantics");

    let a_line = first_line_index_containing(&rendered, "| A |");
    let b_line = first_line_index_containing(&rendered, "| B |");
    let c_line = first_line_index_containing(&rendered, "| C |");
    assert!(
        a_line < b_line && b_line < c_line,
        "connectivity must own rank independently of node declaration order:\n{rendered}"
    );
}
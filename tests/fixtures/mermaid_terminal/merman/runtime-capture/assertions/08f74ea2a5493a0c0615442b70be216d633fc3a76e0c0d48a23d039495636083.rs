fn flowchart_node_label_wrap_width_changes_the_pre_layout_plan() {
    let input = "flowchart LR\nA[\"Alpha Beta Gamma Delta Epsilon\"] --> B[Done]";
    let narrow = render_flowchart(
        input,
        &AsciiRenderOptions::ascii().with_flowchart_node_label_wrap_width(10),
    )
    .expect("custom narrow node-label wrapping should render");
    let wide = render_flowchart(
        input,
        &AsciiRenderOptions::ascii().with_flowchart_node_label_wrap_width(40),
    )
    .expect("custom wide node-label wrapping should render");

    for expected in ["Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Done"] {
        assert!(narrow.contains(expected), "missing {expected:?}:\n{narrow}");
        assert!(wide.contains(expected), "missing {expected:?}:\n{wide}");
    }
    assert!(
        rendered_terminal_width(&narrow) < rendered_terminal_width(&wide),
        "a narrower pre-layout plan should produce a narrower graph:\nnarrow:\n{narrow}\nwide:\n{wide}"
    );
    assert!(
        narrow.lines().count() > wide.lines().count(),
        "a narrower node should gain label rows before routing"
    );
}
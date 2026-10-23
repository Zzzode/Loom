fn flowchart_parser_diamond_shape_renders_as_decision_terminal_shape() {
    let rendered =
        render_flowchart("flowchart LR\nA{A} --> B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(
        rendered,
        "/---\\     +---+\n/   \\     |   |\n< A >---->| B |\n\\   /     |   |\n\\---/     +---+\n"
    );
}
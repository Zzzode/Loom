fn flowchart_parser_edge_length_modifiers_add_spacing() {
    let rendered =
        render_flowchart("flowchart LR\nA ----> B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(
        rendered,
        "+---+         +---+\n|   |         |   |\n| A |-------->| B |\n|   |         |   |\n+---+         +---+\n"
    );
}
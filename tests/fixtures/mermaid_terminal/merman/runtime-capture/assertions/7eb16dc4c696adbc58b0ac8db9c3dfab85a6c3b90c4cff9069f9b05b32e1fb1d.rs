fn flowchart_parser_thick_edges_render_with_heavy_unicode_line() {
    let rendered =
        render_flowchart("flowchart LR\nA ==> B", &AsciiRenderOptions::unicode()).unwrap();

    assert_eq!(
        rendered,
        "┌───┐     ┌───┐\n│   │     │   │\n│ A ┝━━━━►│ B │\n│   │     │   │\n└───┘     └───┘\n"
    );
}
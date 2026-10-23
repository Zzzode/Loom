fn flowchart_parser_unicode_diamond_uses_sloped_shoulders() {
    let rendered =
        render_flowchart("flowchart LR\nA{A} --> B", &AsciiRenderOptions::unicode()).unwrap();

    assert_eq!(
        rendered,
        "╭───╮     ┌───┐\n╱   ╲     │   │\n< A >────►│ B │\n╲   ╱     │   │\n╰───╯     └───┘\n"
    );
}
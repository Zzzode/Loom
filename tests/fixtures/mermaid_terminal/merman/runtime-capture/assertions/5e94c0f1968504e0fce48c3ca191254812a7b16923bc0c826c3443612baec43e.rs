fn flowchart_parser_preserves_mixed_circle_and_cross_markers() {
    let ascii = render_flowchart("flowchart LR\nA o--x B", &AsciiRenderOptions::ascii()).unwrap();
    let unicode =
        render_flowchart("flowchart LR\nA o--x B", &AsciiRenderOptions::unicode()).unwrap();

    assert_eq!(ascii.matches('o').count(), 1, "{ascii}");
    assert_eq!(ascii.matches('x').count(), 1, "{ascii}");
    assert_eq!(unicode.matches('○').count(), 1, "{unicode}");
    assert_eq!(unicode.matches('×').count(), 1, "{unicode}");
}
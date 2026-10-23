fn flowchart_parser_preserves_bidirectional_point_markers_in_both_charsets() {
    let ascii = render_flowchart("flowchart LR\nA <--> B", &AsciiRenderOptions::ascii()).unwrap();
    let unicode =
        render_flowchart("flowchart LR\nA <--> B", &AsciiRenderOptions::unicode()).unwrap();

    assert_eq!(ascii.matches('<').count(), 1, "{ascii}");
    assert_eq!(ascii.matches('>').count(), 1, "{ascii}");
    assert_eq!(unicode.matches('◄').count(), 1, "{unicode}");
    assert_eq!(unicode.matches('►').count(), 1, "{unicode}");
}
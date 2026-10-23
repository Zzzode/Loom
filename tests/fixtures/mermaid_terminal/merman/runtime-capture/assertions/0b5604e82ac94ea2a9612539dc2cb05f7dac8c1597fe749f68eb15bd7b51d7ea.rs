fn flowchart_parser_preserves_labeled_mixed_markers_without_label_overlap() {
    let input = "flowchart LR\nA o-- label --x B";
    let ascii = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();
    let unicode = render_flowchart(input, &AsciiRenderOptions::unicode()).unwrap();

    assert_eq!(ascii.matches('o').count(), 1, "{ascii}");
    assert_eq!(ascii.matches('x').count(), 1, "{ascii}");
    assert!(ascii.contains("label"), "{ascii}");
    assert_eq!(unicode.matches('○').count(), 1, "{unicode}");
    assert_eq!(unicode.matches('×').count(), 1, "{unicode}");
    assert!(unicode.contains("label"), "{unicode}");
}
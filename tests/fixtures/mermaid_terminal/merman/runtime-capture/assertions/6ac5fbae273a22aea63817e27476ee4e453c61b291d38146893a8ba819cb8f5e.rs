fn flowchart_preserves_complete_authored_grapheme_clusters() {
    let source = "flowchart LR\nA[\"Cafe\u{301} \u{1f469}\u{200d}\u{1f4bb} \u{1f1fa}\u{1f1f8}\"]";

    let rendered = render(source, &AsciiRenderOptions::unicode());

    assert!(
        rendered.contains("Cafe\u{301} \u{1f469}\u{200d}\u{1f4bb} \u{1f1fa}\u{1f1f8}"),
        "{rendered}"
    );
}
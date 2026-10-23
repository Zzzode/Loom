fn flowchart_parser_document_variants_render_like_document_shape() {
    for shape in [
        "docs",
        "documents",
        "st-doc",
        "stacked-document",
        "lin-doc",
        "lined-document",
        "tag-doc",
        "tagged-document",
    ] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Doc\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Doc"),
            "{shape} shape should keep its label"
        );
        assert!(
            rendered.contains('~'),
            "{shape} shape should reuse the folded bottom edge:\n{rendered}"
        );
    }
}
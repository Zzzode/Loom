fn flowchart_parser_document_shape_renders() {
    for shape in ["doc", "document"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Doc\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Doc"),
            "document shape should keep its label"
        );
        assert!(
            rendered.contains('~'),
            "document shape should use a folded bottom edge:\n{rendered}"
        );
    }
}
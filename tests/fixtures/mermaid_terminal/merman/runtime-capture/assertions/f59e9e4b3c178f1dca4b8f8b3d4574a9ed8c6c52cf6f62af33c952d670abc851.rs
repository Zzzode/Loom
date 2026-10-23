fn flowchart_parser_lean_left_shape_renders() {
    for shape in ["lean-l", "lean-left", "out-in"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Lean\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Lean"),
            "lean shape should keep its label"
        );
        assert!(
            rendered.contains('\\') && rendered.contains('/'),
            "lean shape should be slanted:\n{rendered}"
        );
    }
}
fn flowchart_parser_lean_right_shape_renders() {
    for shape in ["lean-r", "lean-right", "in-out"] {
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
            rendered.lines().any(|line| line.starts_with('/')),
            "lean shape should be slanted:\n{rendered}"
        );
    }
}
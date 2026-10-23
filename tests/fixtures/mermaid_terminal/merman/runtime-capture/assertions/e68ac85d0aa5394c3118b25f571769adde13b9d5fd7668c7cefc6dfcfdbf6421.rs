fn flowchart_parser_circle_shape_renders_as_circular_terminal_shape() {
    for shape in ["circle", "circ"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"C\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(rendered.contains("C"), "circle shape should keep its label");
        assert!(
            rendered
                .lines()
                .next()
                .is_some_and(|line| line.starts_with('o')),
            "circle shape should use circular corner markers:\n{rendered}"
        );
    }
}
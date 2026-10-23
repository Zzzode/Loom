fn flowchart_parser_trapezoid_shape_renders() {
    for shape in ["trapezoid", "trap-b", "priority", "trapezoid-bottom"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Trap\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Trap"),
            "trapezoid shape should keep its label"
        );
        assert!(
            rendered
                .lines()
                .next()
                .is_some_and(|line| line.starts_with('/')),
            "trapezoid shape should use slanted top corners:\n{rendered}"
        );
    }
}
fn flowchart_parser_hexagon_shape_renders() {
    for shape in ["hexagon", "hex", "prepare"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Hex\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Hex"),
            "hexagon shape should keep its label"
        );
        assert!(
            rendered
                .lines()
                .next()
                .is_some_and(|line| line.starts_with('*')),
            "hexagon shape should use decorated corners:\n{rendered}"
        );
    }
}
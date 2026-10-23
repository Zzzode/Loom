fn flowchart_parser_doublecircle_shape_renders_as_double_terminal_shape() {
    for shape in ["doublecircle", "dbl-circ", "double-circle"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"D\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("D"),
            "double-circle shape should keep its label"
        );
        assert!(
            rendered
                .lines()
                .next()
                .is_some_and(|line| line.starts_with('@')),
            "double-circle shape should use bullseye corner markers:\n{rendered}"
        );
    }
}
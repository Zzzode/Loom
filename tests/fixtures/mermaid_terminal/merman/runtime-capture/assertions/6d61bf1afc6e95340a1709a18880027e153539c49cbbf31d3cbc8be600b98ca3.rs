fn flowchart_parser_inverse_trapezoid_shape_renders() {
    for shape in ["inv-trapezoid", "trap-t", "manual", "trapezoid-top"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Inv\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Inv"),
            "inverse trapezoid shape should keep its label"
        );
        assert!(
            rendered
                .lines()
                .last()
                .is_some_and(|line| line.starts_with('\\')),
            "inverse trapezoid shape should use slanted bottom corners:\n{rendered}"
        );
    }
}
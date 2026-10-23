fn flowchart_parser_ellipse_shape_renders_as_rounded_terminal_shape() {
    let rendered =
        render_flowchart("flowchart LR\nA(-Ellipse-)", &AsciiRenderOptions::ascii()).unwrap();

    assert!(
        rendered.contains("Ellipse"),
        "ellipse shape should keep its label"
    );
    assert!(
        rendered
            .lines()
            .next()
            .is_some_and(|line| line.starts_with('/') && line.ends_with('\\')),
        "ellipse shape should reuse rounded box markers:\n{rendered}"
    );
}
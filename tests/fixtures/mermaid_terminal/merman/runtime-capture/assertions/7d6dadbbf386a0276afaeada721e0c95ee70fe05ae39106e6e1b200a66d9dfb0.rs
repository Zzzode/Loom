fn flowchart_parser_stadium_shape_renders_as_pill_terminal_shape() {
    for shape in ["stadium", "pill", "terminal"] {
        let ascii_rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"S\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            ascii_rendered.contains("S"),
            "stadium shape should keep its label"
        );
        let ascii_first_line = ascii_rendered
            .lines()
            .next()
            .expect("stadium shape should render at least one line");
        assert!(
            ascii_first_line.starts_with('(') && ascii_first_line.ends_with(')'),
            "stadium shape should use pill-style terminal markers:\n{ascii_rendered}"
        );

        let unicode_rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"S\" }}"),
            &AsciiRenderOptions::unicode(),
        )
        .unwrap();
        let unicode_first_line = unicode_rendered
            .lines()
            .next()
            .expect("stadium shape should render at least one line");
        assert!(
            unicode_first_line.starts_with('╭') && unicode_first_line.ends_with('╮'),
            "stadium shape should use rounded corners in unicode mode:\n{unicode_rendered}"
        );
    }
}
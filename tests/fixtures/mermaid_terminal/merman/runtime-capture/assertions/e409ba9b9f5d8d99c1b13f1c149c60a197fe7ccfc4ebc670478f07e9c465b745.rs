fn class_single_line_display_projection_discloses_authored_text() {
    let render_display = |display: &str| {
        let mut model = parse_class_model("classDiagram\nclass A");
        model
            .classes
            .get_mut("A")
            .expect("class A should exist")
            .text = display.to_string();
        render_class_model(&model, &AsciiRenderOptions::ascii())
            .expect("direct class display text should render")
    };

    for (authored, projected_literal, disclosure) in [
        ("\u{1b}", r"\u{1B}", r#"text(bytes=1)="\u{1B}""#),
        ("\n", r"\u{A}", r#"text(bytes=1)="\n""#),
        ("&lt;", "<", r#"text(bytes=4)="&lt;""#),
    ] {
        let transformed = render_display(authored);
        let literal = render_display(projected_literal);

        assert!(
            transformed.contains(disclosure),
            "missing authored display disclosure {disclosure:?}:\n{transformed}"
        );
        assert!(
            transformed.contains(r#"id(bytes=1)="A""#),
            "the fixed class identity should remain disclosed:\n{transformed}"
        );
        assert_ne!(transformed, literal);
    }
}
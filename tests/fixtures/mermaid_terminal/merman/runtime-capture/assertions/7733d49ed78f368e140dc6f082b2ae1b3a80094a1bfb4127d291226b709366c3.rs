fn er_single_line_display_projection_discloses_the_authored_owner() {
    let render_display = |display: &str, as_alias: bool| {
        let mut model = ErDiagramRenderModel::default();
        model.entities.insert(
            "AUTHORED_ID".to_string(),
            ErEntityRenderModel {
                id: "entity-0".to_string(),
                label: if as_alias {
                    "fallback".to_string()
                } else {
                    display.to_string()
                },
                alias: if as_alias {
                    display.to_string()
                } else {
                    String::new()
                },
                ..ErEntityRenderModel::default()
            },
        );
        render_er_model(&model, &AsciiRenderOptions::ascii())
            .expect("direct ER display text should render")
    };

    for (authored, projected_literal, disclosure) in [
        ("\u{1b}", r"\u{1B}", r#"label(bytes=1)="\u{1B}""#),
        ("\n", r"\u{A}", r#"label(bytes=1)="\n""#),
    ] {
        let transformed = render_display(authored, false);
        let literal = render_display(projected_literal, false);

        assert!(
            transformed.contains(disclosure),
            "missing authored display disclosure {disclosure:?}:\n{transformed}"
        );
        assert!(
            transformed.contains(r#"id(bytes=11)="AUTHORED_ID""#),
            "the fixed ER identity should remain disclosed:\n{transformed}"
        );
        assert_ne!(transformed, literal);
    }

    let alias = render_display("\u{1b}", true);
    assert!(
        alias.contains(r#"alias(bytes=1)="\u{1B}""#),
        "the authored alias must own its disclosure row:\n{alias}"
    );
}
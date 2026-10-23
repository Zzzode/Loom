fn er_color_html_wraps_layered_relation_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::NodeBorder, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x202020))
        .with_role(AsciiColorRole::EdgeLine, AsciiRgb::from_hex24(0x303030))
        .with_role(AsciiColorRole::EdgeArrow, AsciiRgb::from_hex24(0x404040))
        .with_role(AsciiColorRole::EdgeLabel, AsciiRgb::from_hex24(0x505050))
        .with_role(AsciiColorRole::Junction, AsciiRgb::from_hex24(0x606060));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::Html)
        .with_color_theme(theme);

    let rendered = render_er("erDiagram\nA ||--|| B : owns\nA ||--|| C : owns", &options)
        .expect("ER should render");

    assert_eq!(
        strip_html_spans(&rendered),
        concat!(
            "     +---+\n",
            "     | A |\n",
            "     +---+\n",
            "      ||\n",
            "  owns owns\n",
            "  +----+---+\n",
            " ||       ||\n",
            "+---+    +---+\n",
            "| B |    | C |\n",
            "+---+    +---+\n",
        )
    );
    for expected_fragment in [
        "<span style=\"color:#101010\">+---+</span>",
        "<span style=\"color:#202020\">A</span>",
        "<span style=\"color:#303030\">----</span>",
        "<span style=\"color:#404040\">||</span>",
        "<span style=\"color:#505050\">owns</span>",
        "<span style=\"color:#606060\">+</span>",
    ] {
        assert!(
            rendered.contains(expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}
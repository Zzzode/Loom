fn flowchart_color_html_wraps_subgraph_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::GroupBorder, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::Section, AsciiRgb::from_hex24(0x202020))
        .with_role(AsciiColorRole::NodeBorder, AsciiRgb::from_hex24(0x303030))
        .with_role(AsciiColorRole::EdgeLine, AsciiRgb::from_hex24(0x404040))
        .with_role(AsciiColorRole::EdgeArrow, AsciiRgb::from_hex24(0x505050))
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x606060));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::Html)
        .with_color_theme(theme);

    let rendered = render_flowchart(
        "flowchart TB\nsubgraph one\ndirection TB\nA --> B\nend",
        &options,
    )
    .unwrap();

    assert_eq!(
        strip_html_spans(&rendered),
        fixture_expected("ascii", "graph_tb_direction.txt")
    );
    for expected_fragment in [
        "<span style=\"color:#101010\">+-------+</span>",
        "<span style=\"color:#202020\">one</span>",
        "<span style=\"color:#303030\">+---+</span>",
        "<span style=\"color:#404040\">|</span>",
        "<span style=\"color:#505050\">v</span>",
        "<span style=\"color:#606060\">A</span>",
        "<span style=\"color:#606060\">B</span>",
    ] {
        assert!(
            rendered.contains(expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}
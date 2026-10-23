fn flowchart_color_truecolor_preserves_roles_after_horizontal_mirror() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::NodeBorder, AsciiRgb::new(7, 7, 7))
        .with_role(AsciiColorRole::Text, AsciiRgb::new(8, 8, 8))
        .with_role(AsciiColorRole::EdgeLine, AsciiRgb::new(9, 9, 9))
        .with_role(AsciiColorRole::EdgeArrow, AsciiRgb::new(10, 10, 10));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::TrueColor)
        .with_color_theme(theme);

    let rendered = render_flowchart("flowchart RL\nA --> B", &options).unwrap();

    assert_eq!(
        strip_ansi(&rendered),
        concat!(
            "+---+     +---+\n",
            "|   |     |   |\n",
            "| B |<----| A |\n",
            "|   |     |   |\n",
            "+---+     +---+\n",
        )
    );
    for expected_code in [
        "\u{1b}[38;2;7;7;7m",
        "\u{1b}[38;2;8;8;8m",
        "\u{1b}[38;2;9;9;9m",
        "\u{1b}[38;2;10;10;10m",
    ] {
        assert!(
            rendered.contains(expected_code),
            "missing {expected_code:?} in {rendered:?}"
        );
    }
}
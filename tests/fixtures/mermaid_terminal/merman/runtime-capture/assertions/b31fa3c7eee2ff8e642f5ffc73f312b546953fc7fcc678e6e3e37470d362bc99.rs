fn class_color_truecolor_marks_dense_relation_summary_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::NodeBorder, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x202020))
        .with_role(AsciiColorRole::Section, AsciiRgb::from_hex24(0x303030))
        .with_role(AsciiColorRole::EdgeLabel, AsciiRgb::from_hex24(0x505050));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::TrueColor)
        .with_color_theme(theme);

    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA --> B : ab\nB --> A : ba\nA --> C : ac\nC --> A : ca\nB --> C : bc\nC --> B : cb",
        &options,
    )
    .expect("dense class diagram should render");

    let ab = framed_class_summary_relation("A", "-->", "B", Some("ab"));
    let ba = framed_class_summary_relation("B", "-->", "A", Some("ba"));
    let ac = framed_class_summary_relation("A", "-->", "C", Some("ac"));
    let ca = framed_class_summary_relation("C", "-->", "A", Some("ca"));
    let bc = framed_class_summary_relation("B", "-->", "C", Some("bc"));
    let cb = framed_class_summary_relation("C", "-->", "B", Some("cb"));
    assert_eq!(
        strip_ansi(&rendered),
        format!(
            concat!(
                "+---+\n",
                "| A |\n",
                "+---+\n",
                "\n",
                "+---+\n",
                "| B |\n",
                "+---+\n",
                "\n",
                "+---+\n",
                "| C |\n",
                "+---+\n",
                "\n",
                "relations:\n",
                "{}\n",
                "{}\n",
                "{}\n",
                "{}\n",
                "{}\n",
                "{}\n",
            ),
            ab, ba, ac, ca, bc, cb,
        )
    );
    for expected_fragment in [
        "\u{1b}[38;2;16;16;16m",
        "\u{1b}[38;2;32;32;32m",
        "\u{1b}[38;2;48;48;48mrelations:",
        "\u{1b}[38;2;80;80;80mid(bytes=1)=\"A\" --> id(bytes=1)=\"B\" : ab",
    ] {
        assert!(
            rendered.contains(expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}
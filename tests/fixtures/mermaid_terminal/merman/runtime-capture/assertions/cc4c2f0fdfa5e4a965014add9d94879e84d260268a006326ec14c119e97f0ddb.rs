fn class_color_html_wraps_parallel_relation_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::NodeBorder, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x202020))
        .with_role(AsciiColorRole::EdgeLine, AsciiRgb::from_hex24(0x303030))
        .with_role(AsciiColorRole::EdgeArrow, AsciiRgb::from_hex24(0x404040))
        .with_role(AsciiColorRole::EdgeLabel, AsciiRgb::from_hex24(0x505050));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::Html)
        .with_color_theme(theme);

    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nAnimal <|-- Dog : parent\nAnimal <|-- Dog : base",
        &options,
    )
    .expect("class diagram should render");

    let parent = framed_class_summary_relation("Animal", "<|--", "Dog", Some("parent"));
    let base = framed_class_summary_relation("Animal", "<|--", "Dog", Some("base"));
    assert_eq!(
        strip_html_spans(&rendered),
        format!(
            concat!(
                "+--------+\n",
                "| Animal |\n",
                "+--------+\n",
                "\n",
                "+-----+\n",
                "| Dog |\n",
                "+-----+\n",
                "\n",
                "relations:\n",
                "{}\n",
                "{}\n",
            ),
            parent, base,
        )
    );
    for expected_fragment in [
        "<span style=\"color:#101010\">+--------+</span>",
        "<span style=\"color:#202020\">Animal</span>",
        "<span style=\"color:#505050\">id(bytes=6)=&quot;Animal&quot; &lt;|-- id(bytes=3)=&quot;Dog&quot; : parent</span>",
        "<span style=\"color:#505050\">id(bytes=6)=&quot;Animal&quot; &lt;|-- id(bytes=3)=&quot;Dog&quot; : base</span>",
    ] {
        assert!(
            rendered.contains(expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}
#[test]
    fn ascii_render_keeps_same_geometry_with_color_disabled() {
        let plain = render_flowchart_fixture_with_options(
            "style-basic.mmd",
            OutputFormat::Ascii,
            TextColorMode::Plain,
        );
        let ansi = render_flowchart_fixture_with_options(
            "style-basic.mmd",
            OutputFormat::Ascii,
            TextColorMode::Ansi,
        );

        assert!(ansi.contains("\u{1b}["));
        assert_eq!(strip_ansi(&ansi), plain);
    }
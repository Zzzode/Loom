#[test]
    fn text_render_uses_stroke_fill_and_label_colors_when_ansi_enabled() {
        let plain = render_flowchart_fixture_with_options(
            "style-basic.mmd",
            OutputFormat::Text,
            TextColorMode::Plain,
        );
        let ansi = render_flowchart_fixture_with_options(
            "style-basic.mmd",
            OutputFormat::Text,
            TextColorMode::Ansi,
        );

        assert!(ansi.contains("38;2;"));
        assert!(ansi.contains("48;2;"));
        assert_eq!(strip_ansi(&ansi), plain);
    }
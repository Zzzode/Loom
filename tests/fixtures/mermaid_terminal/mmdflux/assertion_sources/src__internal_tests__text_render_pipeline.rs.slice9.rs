#[test]
    fn text_render_clears_fill_background_before_right_border() {
        let ansi = render_flowchart_fixture_with_options(
            "style-basic.mmd",
            OutputFormat::Text,
            TextColorMode::Ansi,
        );

        assert!(
            ansi.contains("\u{1b}[38;2;51;51;51;49m│"),
            "expected right border to clear fill background: {ansi:?}"
        );
        assert!(
            !ansi.contains("\u{1b}[48;2;255;238;170m \u{1b}[38;2;51;51;51m│"),
            "right border should not retain fill background: {ansi:?}"
        );
    }
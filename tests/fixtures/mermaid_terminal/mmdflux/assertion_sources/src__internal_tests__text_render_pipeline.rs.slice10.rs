#[test]
    fn text_render_applies_linkstyle_stroke_color_when_ansi_enabled() {
        let input = "graph TD\nA --> B\nB --> C\nlinkStyle default stroke:#999\nlinkStyle 1 stroke:#ff0000,stroke-width:4px\n";
        let plain = crate::render_diagram(
            input,
            OutputFormat::Text,
            &RenderConfig {
                text_color_mode: TextColorMode::Plain,
                ..RenderConfig::default()
            },
        )
        .expect("plain linkStyle text render should succeed");
        let ansi = crate::render_diagram(
            input,
            OutputFormat::Text,
            &RenderConfig {
                text_color_mode: TextColorMode::Ansi,
                ..RenderConfig::default()
            },
        )
        .expect("ansi linkStyle text render should succeed");

        assert!(
            ansi.contains("\u{1b}[38;2;153;153;153m"),
            "default linkStyle stroke color missing: {ansi:?}"
        );
        assert!(
            ansi.contains("\u{1b}[38;2;255;0;0m"),
            "targeted linkStyle stroke color missing: {ansi:?}"
        );
        assert_eq!(strip_ansi(&ansi), plain);
    }
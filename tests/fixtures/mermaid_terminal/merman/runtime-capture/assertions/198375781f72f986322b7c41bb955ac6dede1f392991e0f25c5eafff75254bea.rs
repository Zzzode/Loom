fn every_current_family_normalizes_authored_controls_in_every_encoder() {
    let authored = "before\u{1b}]8;;https://example.invalid\u{7}link\u{9b}after\u{202e}";
    let modes = [
        AsciiColorMode::Plain,
        AsciiColorMode::Ansi16,
        AsciiColorMode::Ansi256,
        AsciiColorMode::TrueColor,
        AsciiColorMode::Html,
    ];

    for (family, source) in CURRENT_ASCII_FAMILY_SOURCES {
        let model = model_with_visible_text(family, source, authored);
        for charset_options in [AsciiRenderOptions::ascii(), AsciiRenderOptions::unicode()] {
            for mode in modes {
                let options = charset_options.with_color_mode(mode);
                let rendered = render_model(&model, &options)
                    .unwrap_or_else(|error| panic!("{family}/{mode:?} failed: {error}"));

                assert_only_renderer_owned_sgr(&rendered, family, mode);
                for control in ['\u{7}', '\u{9b}', '\u{202e}'] {
                    assert!(
                        !rendered.contains(control),
                        "raw control {control:?} leaked from {family}/{mode:?}: {rendered:?}"
                    );
                }
                for visible in [r"\u{1B}", r"\u{7}", r"\u{9B}", r"\u{202E}"] {
                    assert!(
                        rendered.contains(visible),
                        "missing {visible} in {family}/{mode:?}: {rendered:?}"
                    );
                }
            }
        }
    }
}
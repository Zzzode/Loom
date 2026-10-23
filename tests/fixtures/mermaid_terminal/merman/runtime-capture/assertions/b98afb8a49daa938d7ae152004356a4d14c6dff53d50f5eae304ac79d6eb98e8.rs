fn report_metadata_identifies_the_exact_output_encoding() {
    for (color_mode, expected) in [
        (AsciiColorMode::Plain, AsciiOutputEncoding::Plain),
        (AsciiColorMode::Ansi16, AsciiOutputEncoding::Ansi16),
        (AsciiColorMode::Ansi256, AsciiOutputEncoding::Ansi256),
        (AsciiColorMode::TrueColor, AsciiOutputEncoding::TrueColor),
        (AsciiColorMode::Html, AsciiOutputEncoding::Html),
    ] {
        let mut options = AsciiRenderOptions::unicode();
        options.color_mode = color_mode;
        let report = render_report_with_options(
            "flowchart LR\nA[Alpha] --> B[Beta]",
            options,
            AsciiViewportPolicy::default(),
        )
        .unwrap_or_else(|error| panic!("{color_mode:?} report should render: {error}"));

        assert_eq!(report.encoding, expected);
        assert_eq!(report.metadata().encoding, expected.as_str());
    }
}
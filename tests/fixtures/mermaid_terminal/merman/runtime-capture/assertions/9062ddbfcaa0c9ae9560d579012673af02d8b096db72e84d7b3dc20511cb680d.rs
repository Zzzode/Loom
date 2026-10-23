fn styled_output_rejects_a_plain_only_fallback_request_before_rendering() {
    let mut options = AsciiRenderOptions::unicode();
    options.color_mode = AsciiColorMode::Ansi16;
    let error = render_report_with_options(
        "flowchart LR\nA[Alpha] --> B[Beta]",
        options,
        AsciiViewportPolicy::with_max_width(5).overflow(OverflowPolicy::Fallback),
    )
    .expect_err("styled output cannot promise the plain-only complete fallback");

    assert_eq!(
        error,
        AsciiError::InvalidOption {
            field: "ascii_viewport.overflow",
            message: "fallback is not admitted for the selected output encoding",
        }
    );
}
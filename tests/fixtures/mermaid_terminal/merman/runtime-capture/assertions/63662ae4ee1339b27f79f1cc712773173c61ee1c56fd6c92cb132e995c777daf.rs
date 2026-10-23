fn flowchart_ansi16_uses_reset_text_and_sparse_named_accents_without_geometry_changes() {
    let input = "flowchart LR\nA[Client] -->|request| B[Server]";
    let plain = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();
    let rendered = render_flowchart(
        input,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Ansi16),
    )
    .unwrap();

    assert_eq!(strip_ansi(&rendered), plain);
    assert!(
        rendered.contains("\u{1b}[36m"),
        "arrow accent is missing: {rendered:?}"
    );
    assert!(
        !rendered.contains("38;") && !rendered.contains("48;"),
        "ANSI16 must not leak RGB or background guesses: {rendered:?}"
    );
}
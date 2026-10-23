fn sequence_ansi16_preserves_plain_geometry_and_uses_named_title_activation_and_arrow_roles() {
    let input = concat!(
        "sequenceDiagram\n",
        "title Checkout\n",
        "box Group\n",
        "participant A\n",
        "participant B\n",
        "end\n",
        "A->>+B: Start\n",
        "B-->>-A: Done",
    );
    let plain = render_sequence(input, &AsciiRenderOptions::ascii()).unwrap();
    let rendered = render_sequence(
        input,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Ansi16),
    )
    .unwrap();

    assert_eq!(strip_ansi(&rendered), plain);
    assert!(
        rendered.contains("\u{1b}[36m"),
        "title/arrow accent is missing: {rendered:?}"
    );
    assert!(
        rendered.contains("\u{1b}[35m"),
        "activation accent is missing: {rendered:?}"
    );
    assert!(
        !rendered.contains("38;") && !rendered.contains("48;"),
        "ANSI16 must not leak RGB or background guesses: {rendered:?}"
    );
}
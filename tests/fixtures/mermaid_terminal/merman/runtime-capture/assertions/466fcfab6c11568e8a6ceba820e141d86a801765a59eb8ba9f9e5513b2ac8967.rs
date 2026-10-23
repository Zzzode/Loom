fn sequence_box_hsl_fill_truecolor_maps_background_without_plain_text_changes() {
    let input = concat!(
        "sequenceDiagram\n",
        "box hsl(120, 100%, 25%) Group\n",
        "participant A\n",
        "participant B\n",
        "end\n",
        "A->>B: Inside",
    );
    let plain =
        render_sequence(input, &AsciiRenderOptions::ascii()).expect("plain sequence should render");
    let rendered = render_sequence(
        input,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::TrueColor),
    )
    .expect("sequence box hsl fill should render in truecolor mode");

    assert_eq!(strip_ansi(&rendered), plain);
    assert!(
        rendered.contains("\u{1b}[48;2;0;128;0m"),
        "missing sequence box hsl background in {rendered:?}"
    );
}
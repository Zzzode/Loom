fn sequence_box_fill_truecolor_maps_background_without_plain_text_changes() {
    let input =
        "sequenceDiagram\nbox green Group\nparticipant A\nparticipant B\nend\nA->>B: Inside";
    let plain =
        render_sequence(input, &AsciiRenderOptions::ascii()).expect("plain sequence should render");
    let rendered = render_sequence(
        input,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::TrueColor),
    )
    .expect("sequence box fill should render in truecolor mode");

    assert_eq!(strip_ansi(&rendered), plain);
    assert!(
        rendered.contains("\u{1b}[48;2;0;128;0m"),
        "missing sequence box background in {rendered:?}"
    );
    assert!(
        !rendered.contains("\u{1b}[38;2;0;128;0m"),
        "box fill should not be emitted as foreground in {rendered:?}"
    );
}
fn sequence_rect_rgb_color_html_maps_background_without_plain_text_changes() {
    let input =
        "sequenceDiagram\nparticipant A\nparticipant B\nrect rgb(255,238,204)\nA->>B: Shaded\nend";
    let plain =
        render_sequence(input, &AsciiRenderOptions::ascii()).expect("plain sequence should render");
    let rendered = render_sequence(
        input,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html),
    )
    .expect("sequence rect fill should render in HTML color mode");

    assert_eq!(strip_html_spans(&rendered), plain);
    assert!(
        !plain.contains("rgb(255,238,204)"),
        "parseable rect colors should be treated as style, not visible labels:\n{plain}"
    );
    assert!(
        rendered.contains("background-color:#ffeecc"),
        "missing rect background in {rendered:?}"
    );
}
fn sequence_color_truecolor_emits_participant_lifeline_activation_and_message_roles() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::Text, AsciiRgb::new(1, 1, 1))
        .with_role(AsciiColorRole::SequenceFrame, AsciiRgb::new(2, 2, 2))
        .with_role(AsciiColorRole::SequenceLifeline, AsciiRgb::new(3, 3, 3))
        .with_role(AsciiColorRole::SequenceActivation, AsciiRgb::new(4, 4, 4))
        .with_role(AsciiColorRole::EdgeLine, AsciiRgb::new(5, 5, 5))
        .with_role(AsciiColorRole::EdgeArrow, AsciiRgb::new(6, 6, 6))
        .with_role(AsciiColorRole::EdgeLabel, AsciiRgb::new(7, 7, 7))
        .with_role(AsciiColorRole::Junction, AsciiRgb::new(8, 8, 8));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::TrueColor)
        .with_color_theme(theme);

    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>+B: Start\nB-->>-A: Done",
        &options,
    )
    .expect("sequence should render with color roles");

    assert_eq!(
        strip_ansi(&rendered),
        concat!(
            "+---+     +---+\n",
            "| A |     | B |\n",
            "+-+-+     +-+-+\n",
            "  |         |\n",
            "  | Start   |\n",
            "  +-------->|\n",
            "  |         #\n",
            "  | Done    #\n",
            "  |<........+\n",
            "  |         |\n",
        )
    );
    for expected_code in [
        "\u{1b}[38;2;1;1;1m",
        "\u{1b}[38;2;2;2;2m",
        "\u{1b}[38;2;3;3;3m",
        "\u{1b}[38;2;4;4;4m",
        "\u{1b}[38;2;5;5;5m",
        "\u{1b}[38;2;6;6;6m",
        "\u{1b}[38;2;7;7;7m",
        "\u{1b}[38;2;8;8;8m",
    ] {
        assert!(
            rendered.contains(expected_code),
            "missing {expected_code:?} in {rendered:?}"
        );
    }
}
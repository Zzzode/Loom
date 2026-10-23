fn sequence_color_html_wraps_boxes_notes_control_frames_and_messages_without_changing_plain_text() {
    let input = "sequenceDiagram\nbox Group\nparticipant A\nparticipant B\nend\nloop Work\nA->>+B: Start\nNote over A,B: Wait\nB-->>-A: Done\nend";
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::Section, AsciiRgb::from_hex24(0x101010))
        .with_role(
            AsciiColorRole::SequenceFrame,
            AsciiRgb::from_hex24(0x202020),
        )
        .with_role(
            AsciiColorRole::SequenceLifeline,
            AsciiRgb::from_hex24(0x303030),
        )
        .with_role(
            AsciiColorRole::SequenceActivation,
            AsciiRgb::from_hex24(0x404040),
        )
        .with_role(AsciiColorRole::EdgeLine, AsciiRgb::from_hex24(0x505050))
        .with_role(AsciiColorRole::EdgeArrow, AsciiRgb::from_hex24(0x606060))
        .with_role(AsciiColorRole::EdgeLabel, AsciiRgb::from_hex24(0x707070))
        .with_role(AsciiColorRole::Junction, AsciiRgb::from_hex24(0x808080));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::Html)
        .with_color_theme(theme);

    let rendered = render_sequence(input, &options)
        .expect("sequence with boxes, frames, notes, and messages should render");
    let plain = render_sequence(input, &AsciiRenderOptions::ascii())
        .expect("the plain sequence should render with identical geometry");

    assert_eq!(
        strip_html_spans(&rendered),
        plain,
        "HTML color spans must not change control-frame geometry"
    );
    assert_eq!(
        plain,
        concat!(
            "+- Group ----------+\n",
            "| +---+     +---+  |\n",
            "| | A |     | B |  |\n",
            "| +-+-+     +-+-+  |\n",
            "| + loop Work --+  |\n",
            "| | |         | |  |\n",
            "| | | Start   | |  |\n",
            "| | +-------->| |  |\n",
            "| | |         # |  |\n",
            "| |+-----------+|  |\n",
            "| ||   Wait    ||  |\n",
            "| |+-----------+|  |\n",
            "| | |         # |  |\n",
            "| | | Done    # |  |\n",
            "| | |<........+ |  |\n",
            "| +-------------+  |\n",
            "|   |         |    |\n",
            "+------------------+\n",
        )
    );
    for expected_fragment in [
        "<span style=\"color:#202020\">+-</span><span style=\"color:#101010\"> Group </span>",
        "<span style=\"color:#202020\">|</span> <span style=\"color:#202020\">+</span><span style=\"color:#101010\"> loop Work </span>",
        "<span style=\"color:#202020\">|+-----------+|</span>",
        "<span style=\"color:#707070\">Start</span>",
        "<span style=\"color:#505050\">--------</span><span style=\"color:#606060\">&gt;</span>",
        "<span style=\"color:#404040\">#</span>",
        "<span style=\"color:#101010\">Wait</span>",
    ] {
        assert!(
            rendered.contains(expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}
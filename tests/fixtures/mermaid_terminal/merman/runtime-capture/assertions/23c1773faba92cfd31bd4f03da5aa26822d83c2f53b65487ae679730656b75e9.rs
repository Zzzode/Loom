fn issue_53_compact_plain_and_ansi16_share_one_flowchart_geometry() {
    let input = local_semantic_input("flowchart/issue_53_long_node_labels.mmd");
    let compact = AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Compact);
    let plain = render_flowchart(&input, &compact).expect("compact plain fixture should render");
    let ansi16 = render_flowchart(&input, &compact.with_color_mode(AsciiColorMode::Ansi16))
        .expect("compact ANSI16 fixture should render");

    assert_eq!(strip_ansi(&ansi16), plain);
    assert_rectangular_terminal_grid(&plain);
    assert_eq!(
        (
            plain
                .lines()
                .map(terminal_test_width)
                .max()
                .unwrap_or_default(),
            plain.lines().count(),
        ),
        (56, 67)
    );
}
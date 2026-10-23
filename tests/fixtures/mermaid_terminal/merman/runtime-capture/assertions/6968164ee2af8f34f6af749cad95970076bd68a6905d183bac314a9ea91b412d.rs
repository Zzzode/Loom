fn sequence_cjk_half_arrows_preserve_fill_in_reverse_and_narrow_self_paths() {
    let mut options = AsciiRenderOptions::unicode();
    options.terminal_width_profile = TerminalWidthProfile::Cjk;

    let signal_row = |signal: &str| {
        let rendered = render_sequence(
            &format!(
                "sequenceDiagram\nparticipant 客户 服务\nparticipant 数据 库\n数据 库{signal}客户 服务: 同步"
            ),
            &options,
        )
        .unwrap_or_else(|error| panic!("CJK reverse signal {signal:?} should render: {error}"));
        assert!(rendered.contains("客户 服务") && rendered.contains("数据 库"));
        rendered
            .lines()
            .nth(first_line_index_containing(&rendered, "同步") + 1)
            .unwrap_or_else(|| panic!("missing reverse signal row for {signal:?}:\n{rendered}"))
            .to_string()
    };

    let filled = signal_row("-|\\");
    let open = signal_row("-\\\\");
    assert_ne!(filled, open);
    assert!(
        filled.contains("/|"),
        "CJK reverse filled half-arrow should retain its stem:\n{filled}"
    );
    assert!(
        !open.contains("/|"),
        "CJK reverse open half-arrow must remain unfilled:\n{open}"
    );

    options.sequence_self_message_width = 2;
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant 客户 服务\n客户 服务-|\\客户 服务: 自调用",
        &options,
    )
    .expect("a narrow CJK self-message should expand from the shared geometry");
    assert!(rendered.contains("客户 服务") && rendered.contains("自调用"));
    assert!(rendered.contains("/|"));
}
fn sequence_wide_messages_keep_lifelines_in_both_width_profiles() {
    for profile in [TerminalWidthProfile::Unicode, TerminalWidthProfile::Cjk] {
        let mut options = AsciiRenderOptions::ascii();
        options.terminal_width_profile = profile;
        let rendered = render_sequence(
            "sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\nC->>A: ·数据数据数据数据",
            &options,
        ).expect("wide message label must render");
        assert_label_lifelines(&rendered, "·数据数据数据数据", &['A', 'B', 'C'], &options);
    }
}
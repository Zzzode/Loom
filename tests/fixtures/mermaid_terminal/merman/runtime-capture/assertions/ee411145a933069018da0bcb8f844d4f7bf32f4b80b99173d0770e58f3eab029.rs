fn sequence_ascii_half_arrows_preserve_filled_and_open_semantics() {
    let signal_row = |signal: &str| {
        let rendered = render_sequence(
            &format!("sequenceDiagram\nparticipant A\nparticipant B\nA{signal}B: Same"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("{signal:?} should render: {error}"));
        rendered
            .lines()
            .nth(first_line_index_containing(&rendered, "Same") + 1)
            .unwrap_or_else(|| panic!("missing signal row for {signal:?}:\n{rendered}"))
            .to_string()
    };

    for (filled, open, filled_glyph) in [
        ("-|\\", "-\\\\", "|\\"),
        ("-|/", "-//", "|/"),
        ("/|-", "//-", "/|"),
        ("\\|-", "\\\\-", "\\|"),
        ("--|\\", "--\\\\", "|\\"),
        ("--|/", "--//", "|/"),
        ("/|--", "//--", "/|"),
        ("\\|--", "\\\\--", "\\|"),
    ] {
        let filled_row = signal_row(filled);
        let open_row = signal_row(open);
        assert_ne!(
            filled_row, open_row,
            "filled and open half arrows must not collide for {filled:?}/{open:?}"
        );
        assert!(
            filled_row.contains(filled_glyph),
            "filled half arrow {filled:?} should render {filled_glyph:?}:\n{filled_row}"
        );
        assert!(
            !open_row.contains(filled_glyph),
            "open half arrow {open:?} must not synthesize a filled stem:\n{open_row}"
        );
    }
}
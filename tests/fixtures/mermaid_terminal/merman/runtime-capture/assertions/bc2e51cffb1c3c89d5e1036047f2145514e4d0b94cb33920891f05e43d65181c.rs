fn sequence_open_line_types_render_as_headless_signals() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->B: Headless\nA-->B: Dotted\nB->A: Back",
        &AsciiRenderOptions::unicode(),
    )
    .expect("headless sequence messages should render");
    let lines = rendered.lines().collect::<Vec<_>>();
    let signal_row_after = |label| {
        lines
            .get(first_line_index_containing(&rendered, label) + 1)
            .copied()
            .unwrap_or_else(|| panic!("missing signal row after {label:?}:\n{rendered}"))
    };
    let solid = signal_row_after("Headless");
    let dotted = signal_row_after("Dotted");
    let right_to_left = signal_row_after("Back");

    assert!(
        solid.contains('├') && solid.contains('─') && solid.contains('│'),
        "solid open line type should retain its stroke without an endpoint marker:\n{rendered}"
    );
    assert!(
        dotted.contains('├') && dotted.contains('┈') && dotted.contains('│'),
        "dotted open line type should retain its dotted stroke without a marker:\n{rendered}"
    );
    assert!(
        right_to_left.contains('│') && right_to_left.contains('─') && right_to_left.contains('┤'),
        "right-to-left open line type should remain headless:\n{rendered}"
    );
    assert!(
        !rendered
            .chars()
            .any(|ch| matches!(ch, '<' | '>' | '►' | '◄')),
        "headless signals must not synthesize ASCII or Unicode arrowheads:\n{rendered}"
    );
}
fn sequence_local_semantic_fixture_covers_multiple_reference_messages() {
    let input = read_local_semantic_fixture("sequence/multiple_messages.mmd");
    let rendered = render_sequence(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic multiple-message fixture should render");

    for expected in [
        "Alice", "Bob", "Charlie", "Hello", "Forward", "Reply", "Done",
    ] {
        assert!(
            rendered.contains(expected),
            "sequence multiple-message fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    assert!(
        first_line_index_containing(&rendered, "Hello")
            < first_line_index_containing(&rendered, "Forward"),
        "sequence messages should preserve source order before the cross-participant reply:\n{rendered}"
    );
    assert!(
        first_line_index_containing(&rendered, "Reply")
            < first_line_index_containing(&rendered, "Done"),
        "sequence replies should preserve source order:\n{rendered}"
    );
}
fn sequence_local_semantic_fixture_covers_self_messages_with_notes_and_alt_branch() {
    let input = read_local_semantic_fixture("sequence/self_messages_with_notes.mmd");

    let rendered = render_sequence(&input, &AsciiRenderOptions::unicode())
        .expect("self-message local semantic sequence fixture should render");

    for expected in [
        "Main Process",
        "Renderer",
        "3s Fallback Timer",
        "Multiple panels",
        "Single panel",
        "closePanel(focusedId)",
        "closePanel(lastId)",
        "Panel removed",
        "Stack becomes []",
        "Panel reopens",
        "window.destroy()",
    ] {
        assert!(
            rendered.contains(expected),
            "self-message semantic sequence fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        first_line_index_containing(&rendered, "Multiple panels")
            < first_line_index_containing(&rendered, "Single panel"),
        "alt branch order should remain readable in the semantic fixture:\n{rendered}"
    );
    assert!(
        first_line_index_containing(&rendered, "Panel removed")
            < first_line_index_containing(&rendered, "Panel reopens"),
        "branch-local note ordering should stay visible:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 10,
        "self-message semantic sequence fixture should produce a multi-line layout:\n{rendered}"
    );
}
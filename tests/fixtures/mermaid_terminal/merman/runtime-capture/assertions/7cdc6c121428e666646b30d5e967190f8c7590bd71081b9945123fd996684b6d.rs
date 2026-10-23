fn state_note_edges_render_without_arrowheads() {
    let rendered = render_state(
        "stateDiagram-v2\nS1\nnote right of S1 : note text",
        &AsciiRenderOptions::ascii(),
    )
    .expect("state note edges should render as open connectors");

    assert!(
        rendered.contains("S1") && rendered.contains("note text"),
        "state and note should both render:\n{rendered}"
    );
    assert!(
        !rendered
            .chars()
            .any(|ch| matches!(ch, '>' | '<' | '^' | 'v')),
        "note-only state output should not contain arrowheads:\n{rendered}"
    );
}
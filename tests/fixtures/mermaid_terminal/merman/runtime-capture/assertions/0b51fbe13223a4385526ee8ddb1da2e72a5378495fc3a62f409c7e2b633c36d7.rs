fn state_notes_render_as_note_nodes() {
    let rendered = render_state(
        "stateDiagram-v2\nA --> B\nnote right of A : note text",
        &AsciiRenderOptions::ascii(),
    )
    .expect("state notes should render as terminal note nodes");

    assert!(
        rendered.contains("note text"),
        "note text should render in the ASCII output:\n{rendered}"
    );
    assert!(
        !rendered.contains("----note") && !rendered.contains("----parent"),
        "state note implementation ids should not leak into ASCII output:\n{rendered}"
    );
}
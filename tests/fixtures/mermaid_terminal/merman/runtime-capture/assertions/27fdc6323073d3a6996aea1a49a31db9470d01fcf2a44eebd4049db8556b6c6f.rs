fn state_block_notes_render_multiline_note_nodes() {
    let rendered = render_state(
        "stateDiagram-v2\nA --> B\nnote right of A\n  line1\n  line2\nend note",
        &AsciiRenderOptions::ascii(),
    )
    .expect("state block notes should render as multiline terminal note nodes");

    assert!(
        rendered.contains("line1") && rendered.contains("line2"),
        "block note lines should render in the ASCII output:\n{rendered}"
    );
}
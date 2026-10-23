fn state_multiple_notes_preserve_text_and_side_ownership() {
    let rendered = render_state(
        concat!(
            "stateDiagram-v2\n",
            "A\n",
            "note left of A : left text\n",
            "note right of A : right text\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("multiple notes on one state should render");

    assert!(
        rendered.contains("left text") && rendered.contains("right text"),
        "both note payloads must survive typed projection:\n{rendered}"
    );
    let left = text_position(&rendered, "left text");
    let state = text_position(&rendered, " A ");
    let right = text_position(&rendered, "right text");
    assert_eq!(
        left.1, state.1,
        "left note and state should share a horizontal lane:\n{rendered}"
    );
    assert_eq!(
        state.1, right.1,
        "state and right note should share a horizontal lane:\n{rendered}"
    );
    assert!(
        left.0 < state.0 && state.0 < right.0,
        "note side constraints must determine physical placement:\n{rendered}"
    );
}
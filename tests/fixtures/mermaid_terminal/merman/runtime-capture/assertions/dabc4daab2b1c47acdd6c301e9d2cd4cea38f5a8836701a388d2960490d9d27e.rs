fn sequence_numbered_and_normalized_messages_use_the_measured_spelling() {
    let options = AsciiRenderOptions::unicode();
    let mut model = parse_sequence_render_model(
        "sequenceDiagram\nautonumber\nparticipant A\nparticipant B\nA->>B: find user by email",
    );
    model
        .messages
        .iter_mut()
        .find(|message| message.from.is_some())
        .expect("the typed model must contain a signal")
        .message = SequenceMessagePayload::Text("find user\nby email".to_string());
    let rendered = render_sequence_model(&model, &options)
        .expect("numbered terminal-normalized label must render");
    assert_label_lifelines(
        &rendered,
        "1. find user\\u{A}by email",
        &['A', 'B'],
        &options,
    );
}
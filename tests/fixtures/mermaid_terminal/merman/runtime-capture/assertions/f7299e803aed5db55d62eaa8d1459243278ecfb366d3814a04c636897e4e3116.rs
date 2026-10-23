fn sequence_multiline_notes_render_from_typed_model() {
    let mut model = basic_sequence_model();
    model.notes.push(SequenceNote {
        actor: "A".into(),
        message: "line 1\nline 2".to_string(),
        placement: 1,
        wrap: false,
    });
    model.messages.push(SequenceMessage {
        id: "n0".to_string(),
        from: Some("A".to_string()),
        to: Some("A".to_string()),
        message_type: 2,
        message: SequenceMessagePayload::Text("line 1\nline 2".to_string()),
        wrap: false,
        activate: false,
        placement: Some(1),
        central_connection: 0,
    });

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::ascii())
        .expect("sequence should render");
    let lines = normalize_sequence_output(&rendered)
        .lines()
        .map(ToOwned::to_owned)
        .collect::<Vec<_>>();
    let line_1 = lines
        .iter()
        .position(|line| line.contains("line 1"))
        .expect("first note line should render");
    let line_2 = lines
        .iter()
        .position(|line| line.contains("line 2"))
        .expect("second note line should render");

    assert_eq!(
        line_2,
        line_1 + 1,
        "multiline notes should render as adjacent note content rows:\n{rendered}"
    );
}
fn sequence_note_projection_must_match_the_typed_note_facts() {
    let mut base = basic_sequence_model();
    add_sequence_participant(&mut base, "B");
    base.notes.push(SequenceNote {
        actor: vec!["A", "B"].into(),
        message: "typed note".to_string(),
        placement: 2,
        wrap: true,
    });
    base.messages.push(SequenceMessage {
        id: "n0".to_string(),
        from: Some("A".to_string()),
        to: Some("B".to_string()),
        message_type: 2,
        message: SequenceMessagePayload::Text("typed note".to_string()),
        wrap: true,
        activate: false,
        placement: Some(2),
        central_connection: 0,
    });

    let mut wrong_text = base.clone();
    wrong_text.messages[0].message = SequenceMessagePayload::Text("other note".to_string());

    let mut wrong_actor = base.clone();
    wrong_actor.messages[0].to = Some("A".to_string());

    let mut wrong_placement = base.clone();
    wrong_placement.messages[0].placement = Some(1);

    let mut missing_message = base.clone();
    missing_message.messages.clear();

    for model in [wrong_text, wrong_actor, wrong_placement, missing_message] {
        assert_unsupported_sequence_model(model, "note model consistency");
    }
}
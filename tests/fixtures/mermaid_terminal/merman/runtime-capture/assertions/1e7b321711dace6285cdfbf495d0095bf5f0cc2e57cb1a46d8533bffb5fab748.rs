fn sequence_activated_signals_must_bind_their_target_state_event() {
    let mut missing = basic_sequence_model();
    add_sequence_participant(&mut missing, "B");
    missing.messages.push(SequenceMessage {
        activate: true,
        ..message(Some("A"), Some("B"), 0)
    });

    let mut wrong_actor = missing.clone();
    let mut wrong_start = message(Some("A"), None, 17);
    wrong_start.message = SequenceMessagePayload::Text(String::new());
    wrong_actor.messages.push(wrong_start);

    for model in [missing, wrong_actor] {
        assert_unsupported_sequence_model(model, "activation state events");
    }

    let mut valid = basic_sequence_model();
    add_sequence_participant(&mut valid, "B");
    valid.messages.push(SequenceMessage {
        activate: true,
        ..message(Some("A"), Some("B"), 0)
    });
    let mut start = message(Some("B"), None, 17);
    start.message = SequenceMessagePayload::Text(String::new());
    valid.messages.push(start);
    render_sequence_model(&valid, &AsciiRenderOptions::unicode())
        .expect("a matching activation record should satisfy direct-model admission");
}
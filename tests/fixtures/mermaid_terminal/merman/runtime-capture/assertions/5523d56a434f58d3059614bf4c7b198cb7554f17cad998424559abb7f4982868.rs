fn sequence_message_payload_must_match_its_semantic_kind() {
    let autonumber = SequenceMessagePayload::Autonumber(SequenceAutonumber {
        start: Some(5.0),
        step: Some(2.0),
        visible: true,
    });

    let mut signal_payload = basic_sequence_model();
    let mut signal = message(Some("A"), Some("A"), 0);
    signal.message = autonumber.clone();
    signal_payload.messages.push(signal);

    let mut note_payload = basic_sequence_model();
    let mut note = message(Some("A"), Some("A"), 2);
    note.message = autonumber;
    note.placement = Some(1);
    note_payload.messages.push(note);

    let mut autonumber_payload = basic_sequence_model();
    autonumber_payload
        .messages
        .push(message(None, None, LINETYPE_AUTONUMBER));

    for model in [signal_payload, note_payload, autonumber_payload] {
        assert_unsupported_sequence_model(model, "message payload shape");
    }
}
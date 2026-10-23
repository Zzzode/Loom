fn sequence_structural_message_records_reject_unconsumed_fields() {
    let mut activation_with_target = basic_sequence_model();
    let mut activation = message(Some("A"), Some("A"), 17);
    activation.message = SequenceMessagePayload::Text(String::new());
    activation_with_target.messages.push(activation);

    let mut activation_with_text = basic_sequence_model();
    activation_with_text
        .messages
        .push(message(Some("A"), None, 17));

    let mut control_end_with_text = basic_sequence_model();
    control_end_with_text
        .messages
        .push(message(None, None, LINETYPE_LOOP_END));

    let mut note_without_placement = basic_sequence_model();
    note_without_placement
        .messages
        .push(message(Some("A"), Some("A"), 2));

    for model in [
        activation_with_target,
        activation_with_text,
        control_end_with_text,
        note_without_placement,
    ] {
        assert_unsupported_sequence_model(model, "message record shape");
    }
}
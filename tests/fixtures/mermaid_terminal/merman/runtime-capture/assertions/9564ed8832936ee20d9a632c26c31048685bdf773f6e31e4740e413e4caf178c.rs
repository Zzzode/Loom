fn sequence_rect_par_over_malformed_ordering_is_explicitly_unsupported() {
    let mut cases = Vec::new();

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model
        .messages
        .push(message(None, None, LINETYPE_RECT_START));
    model.messages.push(message(Some("A"), Some("B"), 0));
    model.messages.push(empty_message(LINETYPE_PAR_END));
    cases.push(model);

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model
        .messages
        .push(message(None, None, LINETYPE_PAR_OVER_START));
    model.messages.push(message(Some("A"), Some("B"), 0));
    model.messages.push(empty_message(LINETYPE_RECT_END));
    cases.push(model);

    for model in cases {
        assert_unsupported_sequence_model(model, "control block ordering");
    }
}
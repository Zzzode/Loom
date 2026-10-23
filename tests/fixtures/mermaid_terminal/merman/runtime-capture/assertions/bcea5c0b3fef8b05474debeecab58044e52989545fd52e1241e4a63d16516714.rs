fn sequence_central_records_must_bind_their_visible_signal() {
    let central_record = || {
        let mut record = message(Some("A"), None, 59);
        record.message = SequenceMessagePayload::Text(String::new());
        record
    };

    let mut orphan = basic_sequence_model();
    orphan.messages.push(central_record());
    assert_unsupported_sequence_model(orphan, "central connection records");

    let mut missing = basic_sequence_model();
    let mut decorated = message(Some("A"), Some("A"), 0);
    decorated.central_connection = 59;
    decorated.activate = true;
    missing.messages.push(decorated.clone());
    assert_unsupported_sequence_model(missing, "central connection records");

    let mut valid = basic_sequence_model();
    valid.messages.push(decorated);
    valid.messages.push(central_record());
    let rendered = render_sequence_model(&valid, &AsciiRenderOptions::unicode())
        .expect("a central record bound to its visible signal should render");
    assert!(rendered.contains("Hi") && rendered.contains('○'));
}
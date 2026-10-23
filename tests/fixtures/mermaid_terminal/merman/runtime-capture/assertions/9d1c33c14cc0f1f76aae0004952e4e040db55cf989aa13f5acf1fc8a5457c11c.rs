fn sequence_note_messages_are_the_compatibility_source_when_notes_are_absent() {
    let mut model = basic_sequence_model();
    let mut note = message(Some("A"), Some("A"), 2);
    note.message = SequenceMessagePayload::Text("message-owned note".to_string());
    note.placement = Some(1);
    model.messages.push(note);

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::ascii())
        .expect("ordered note messages should remain drawable without the duplicate notes vector");
    assert!(rendered.contains("message-owned note"));
}
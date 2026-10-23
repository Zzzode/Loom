fn sequence_note_after_destroy_uses_the_static_actor_anchor() {
    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.messages.push(message(Some("A"), Some("B"), 0));
    model.destroyed_actors.insert("B".to_string(), 0);

    let mut note = message(Some("B"), Some("B"), 2);
    note.message = SequenceMessagePayload::Text("After destroy".to_string());
    note.placement = Some(1);
    model.messages.push(note);

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::unicode())
        .expect("notes should retain static anchors after actor destruction");

    assert!(
        rendered.contains('×') && rendered.contains("After destroy"),
        "destroy marker and later anchored note should both remain visible:\n{rendered}"
    );
}
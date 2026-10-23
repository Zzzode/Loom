fn sequence_wrapped_actor_labels_render_multiline_participant_boxes() {
    let mut model = basic_sequence_model();
    let actor = model.actors.get_mut("A").unwrap();
    actor.description = "Public API Gateway".to_string();
    actor.wrap = true;

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::ascii())
        .expect("sequence should render");
    let normalized = normalize_sequence_output(&rendered);
    let lines = normalized.lines().collect::<Vec<_>>();

    assert_eq!(lines[0], "+------------+");
    assert_eq!(lines[1], "| Public API |");
    assert_eq!(lines[2], "|  Gateway   |");
    assert_eq!(lines[3], "+------+-----+");
    assert_eq!(lines[4], "       |");
}
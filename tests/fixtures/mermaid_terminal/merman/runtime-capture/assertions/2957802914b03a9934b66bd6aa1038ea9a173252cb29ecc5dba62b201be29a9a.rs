fn sequence_boxes_with_unknown_actors_are_explicitly_unsupported() {
    let mut model = basic_sequence_model();
    model.boxes.push(SequenceBox {
        actor_keys: vec!["B".to_string()],
        fill: "green".to_string(),
        name: Some("Group".to_string()),
        wrap: false,
    });

    assert_unsupported_sequence_model(model, "boxes with unknown actors");
}
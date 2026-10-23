fn sequence_actor_lifecycle_validates_hand_built_indices() {
    let mut cases = Vec::new();

    let mut model = basic_sequence_model();
    model.messages.push(message(Some("A"), Some("A"), 0));
    model.created_actors.insert("B".to_string(), 0);
    cases.push((model, "actor lifecycle actors"));

    let mut model = basic_sequence_model();
    model.messages.push(message(Some("A"), Some("A"), 0));
    model.created_actors.insert("A".to_string(), 2);
    cases.push((model, "actor lifecycle message indices"));

    let mut model = basic_sequence_model();
    model.messages.push(message(Some("A"), Some("A"), 0));
    model.created_actors.insert("A".to_string(), 1);
    cases.push((model, "actor creation messages"));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.messages.push(message(Some("A"), Some("A"), 0));
    model.created_actors.insert("B".to_string(), 0);
    cases.push((model, "actor creation messages"));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.messages.push(message(Some("A"), Some("A"), 0));
    model.destroyed_actors.insert("B".to_string(), 0);
    cases.push((model, "actor destruction messages"));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.messages.push(message(Some("A"), Some("B"), 0));
    model.messages.push(message(Some("A"), Some("B"), 0));
    model.destroyed_actors.insert("B".to_string(), 0);
    cases.push((model, "actor lifecycle visibility"));

    for (model, feature) in cases {
        assert_unsupported_sequence_model(model, feature);
    }
}
fn sequence_unknown_actor_types_are_explicitly_unsupported() {
    let mut model = basic_sequence_model();
    model.actors.get_mut("A").unwrap().actor_type = "gateway".to_string();

    assert_unsupported_sequence_model(model, "actor types");
}
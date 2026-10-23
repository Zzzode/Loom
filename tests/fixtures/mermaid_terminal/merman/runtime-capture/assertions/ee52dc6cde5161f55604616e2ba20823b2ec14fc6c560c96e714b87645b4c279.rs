fn sequence_actor_order_must_be_a_complete_actor_permutation() {
    let mut unknown = basic_sequence_model();
    unknown.actor_order = vec!["missing".to_string()];

    let mut duplicate = basic_sequence_model();
    add_sequence_participant(&mut duplicate, "B");
    duplicate.actor_order = vec!["A".to_string(), "A".to_string()];

    let mut omitted = basic_sequence_model();
    add_sequence_participant(&mut omitted, "B");
    omitted.actor_order = vec!["A".to_string()];

    for model in [unknown, duplicate, omitted] {
        assert_unsupported_sequence_model(model, "actor order");
    }
}
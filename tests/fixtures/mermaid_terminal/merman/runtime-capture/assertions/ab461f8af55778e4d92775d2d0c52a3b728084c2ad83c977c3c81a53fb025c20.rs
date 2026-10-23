fn sequence_actor_lifecycle_accepts_consecutive_same_kind_declarations() {
    for (input, consumed_actor, superseded_actor, created) in [
        (
            concat!(
                "sequenceDiagram\n",
                "participant A\n",
                "create participant B\n",
                "create participant C\n",
                "A->>C: create\n",
            ),
            "C",
            "B",
            true,
        ),
        (
            concat!(
                "sequenceDiagram\n",
                "participant A\n",
                "participant B\n",
                "destroy A\n",
                "destroy B\n",
                "A--xB: destroy\n",
            ),
            "B",
            "A",
            false,
        ),
    ] {
        let model = parse_sequence_render_model(input);
        let consumed_index = if created {
            model.created_actor_message_index(consumed_actor)
        } else {
            model.destroyed_actor_message_index(consumed_actor)
        };
        let superseded_index = if created {
            model.created_actor_message_index(superseded_actor)
        } else {
            model.destroyed_actor_message_index(superseded_actor)
        };
        assert_eq!(consumed_index, Some(0));
        assert_eq!(superseded_index, None);

        let rendered = render_sequence_model(&model, &AsciiRenderOptions::unicode())
            .expect("the latest pending lifecycle declaration should own the signal");
        assert!(
            rendered.contains("create") || rendered.contains("destroy"),
            "the consuming lifecycle signal must remain visible:\n{rendered}"
        );
    }
}
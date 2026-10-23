fn sequence_actor_lifecycle_finds_the_signal_after_intervening_records() {
    for (input, actor, expected_label, expected_signal) in [
        (
            concat!(
                "sequenceDiagram\n",
                "participant A\n",
                "create participant B\n",
                "Note over A: pending\n",
                "autonumber\n",
                "A->>B: ready\n",
            ),
            "B",
            "pending",
            "ready",
        ),
        (
            concat!(
                "sequenceDiagram\n",
                "participant A\n",
                "participant B\n",
                "destroy B\n",
                "Note over A: closing\n",
                "autonumber\n",
                "A--xB: bye\n",
            ),
            "B",
            "closing",
            "bye",
        ),
    ] {
        let model = parse_sequence_render_model(input);
        let lifecycle_index = model
            .created_actors
            .get(actor)
            .or_else(|| model.destroyed_actors.get(actor));
        assert_eq!(lifecycle_index, Some(&0));
        assert_eq!(model.messages[0].message_type, 2, "the anchor is a note");
        assert_eq!(
            model.messages[1].message_type, LINETYPE_AUTONUMBER,
            "autonumber is also retained before the associated signal"
        );

        let rendered = render_sequence_model(&model, &AsciiRenderOptions::unicode())
            .expect("lifecycle endpoint validation must use the later associated signal");
        assert!(
            rendered.contains(expected_label) && rendered.contains(expected_signal),
            "intervening records and the associated lifecycle signal must remain visible:\n{rendered}"
        );
    }
}
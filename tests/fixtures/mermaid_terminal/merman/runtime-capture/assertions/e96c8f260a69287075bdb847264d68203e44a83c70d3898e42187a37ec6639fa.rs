fn sequence_semantic_projection_observes_cancellation_during_streaming_properties() {
        let parsed = merman_core::Engine::new()
            .parse_diagram_for_render_model_sync(
                "sequenceDiagram\nparticipant A",
                merman_core::ParseOptions::strict(),
            )
            .expect("sequence diagram should parse")
            .expect("sequence diagram should be detected");
        let (metadata, mut model) = parsed.into_parts();
        let merman_core::diagram::RenderSemanticModel::Sequence(sequence) = &mut model else {
            unreachable!("fixture is a sequence diagram");
        };
        sequence
            .actors
            .get_mut("A")
            .expect("fixture actor should exist")
            .properties
            .insert(
                "payload".to_string(),
                Value::Array((0..4_096).map(Value::from).collect()),
            );
        let control = merman_core::OperationControl::new();
        control.cancel_after_checkpoints(64);
        let resources = crate::AsciiResourcePolicy::unbounded();
        let execution = crate::operation::AsciiExecution::new(&control, &resources);

        let result = semantic_fallback_projection(&model, &metadata, &control, execution);

        assert!(matches!(
            result,
            Err(SemanticFallbackError::Cancelled(cancelled))
                if cancelled.phase == OperationPhase::Semantic
        ));
    }
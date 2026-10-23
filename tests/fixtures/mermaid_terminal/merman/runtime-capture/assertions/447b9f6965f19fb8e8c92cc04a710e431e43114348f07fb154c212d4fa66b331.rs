fn sequence_projection_cancellation_precedes_the_next_semantic_work_charge() {
        let model = parse_sequence_model("sequenceDiagram\nparticipant A\nparticipant B\n");
        let policy = AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, 1)
            .expect("one semantic work unit should be a valid limit");
        let mut resources = ResourceContext::new(policy);
        let control = OperationControl::new();
        control.cancel_after_checkpoints(1);

        let error = from_sequence_model(
            &model,
            AsciiRenderOptions::unicode().sequence_layout(),
            &mut resources,
            AsciiExecution::new(&control, &policy),
        )
        .expect_err("scheduled semantic cancellation should beat the limit-minus-one charge");

        assert!(matches!(
            error,
            AsciiError::Cancelled(cancelled)
                if cancelled.phase == OperationPhase::Semantic
                    && cancelled.reason == merman_core::CancelReason::Requested
        ));
        assert_eq!(resources.layout_work_used(), 0);
    }
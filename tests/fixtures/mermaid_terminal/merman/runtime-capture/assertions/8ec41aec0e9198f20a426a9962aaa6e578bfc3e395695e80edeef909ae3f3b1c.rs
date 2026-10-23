fn sequence_projection_failure_restores_the_complete_shared_ledger() {
        let model = parse_sequence_model("sequenceDiagram\nparticipant A as AB\n");
        let policy = AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxGridCells, 1)
            .expect("one grid cell should be a valid limit");
        let mut resources = ResourceContext::new(policy);
        resources
            .charge_layout_work(5)
            .expect("the pre-existing work debit should fit");
        resources
            .charge_document_cells(3)
            .expect("the pre-existing document debit should fit");
        let control = OperationControl::new();

        let error = from_sequence_model(
            &model,
            AsciiRenderOptions::unicode().sequence_layout(),
            &mut resources,
            AsciiExecution::new(&control, &policy),
        )
        .expect_err("the participant label should exceed the one-cell grid limit");

        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxGridCells
                    && details.actual == 2
                    && details.max == 1
        ));
        assert_eq!(resources.layout_work_used(), 5);
        assert_eq!(resources.document_cells_used(), 3);
    }
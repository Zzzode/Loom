fn render_flowchart_respects_grid_cell_limit() {
        let mut model = empty_flowchart();
        model.nodes = vec![node("A"), node("B")];
        model.edges = vec![edge("A", "B")];
        let options = AsciiRenderOptions::ascii();
        let mut resources = AsciiResourcePolicy::default();
        resources
            .apply_limit(AsciiResourceLimitId::MaxGridCells, 1)
            .unwrap();

        let err = render_flowchart_with_resources(&model, &options, resources).unwrap_err();

        assert!(matches!(
            err,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxGridCells
                    && details.actual > details.max
                    && details.max == 1
        ));
    }
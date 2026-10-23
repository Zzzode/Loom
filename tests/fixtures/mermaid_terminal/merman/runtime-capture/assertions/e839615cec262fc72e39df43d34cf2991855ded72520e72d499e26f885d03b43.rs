fn flowchart_semantic_fallback_observes_cancellation_before_projection() {
        let parsed = merman_core::Engine::new()
            .parse_diagram_for_render_model_sync(
                "flowchart LR\nA[Alpha] --> B[Beta]",
                merman_core::ParseOptions::strict(),
            )
            .expect("flowchart should parse")
            .expect("flowchart should be detected");
        let (metadata, model) = parsed.into_parts();
        let control = merman_core::OperationControl::new();
        control.cancel_after_checkpoints(0);
        let resources = crate::AsciiResourcePolicy::default();
        let execution = crate::operation::AsciiExecution::new(&control, &resources);

        let error = build_semantic_fallback(
            &model,
            &metadata,
            AsciiExtent::new(80, 5),
            OutputBuildContext {
                color_mode: AsciiColorMode::Plain,
                profile: TerminalWidthProfile::Unicode,
                layout_profile: crate::AsciiLayoutProfile::Canonical,
                policy: AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
                execution,
            },
        )
        .expect_err("cancelled fallback must not emit a partial projection");

        assert!(matches!(
            error,
            AsciiError::Cancelled(cancelled)
                if cancelled.phase == OperationPhase::Semantic
        ));
    }
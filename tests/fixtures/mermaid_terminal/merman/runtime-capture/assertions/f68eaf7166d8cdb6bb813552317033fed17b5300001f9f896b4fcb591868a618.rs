fn sequence_semantic_fallback_honors_exact_output_budget() {
        let parsed = merman_core::Engine::new()
            .parse_diagram_for_render_model_sync(
                concat!(
                    "sequenceDiagram\n",
                    "participant A\n",
                    "properties A: {\"payload\": {}}\n",
                    "participant B\n",
                    "A->>B: A long authored message forcing a wide primary projection",
                ),
                merman_core::ParseOptions::strict(),
            )
            .expect("sequence diagram should parse")
            .expect("sequence diagram should be detected");
        let (metadata, model) = parsed.into_parts();
        let unbounded_resources = crate::AsciiResourcePolicy::unbounded();
        let unbounded_execution = crate::operation::AsciiExecution::for_test(&unbounded_resources);
        let unbounded = build_semantic_fallback(
            &model,
            &metadata,
            AsciiExtent::new(80, 5),
            OutputBuildContext {
                color_mode: AsciiColorMode::Plain,
                profile: TerminalWidthProfile::Unicode,
                layout_profile: crate::AsciiLayoutProfile::Canonical,
                policy: AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
                execution: unbounded_execution,
            },
        )
        .expect("unbounded sequence fallback should render");
        let flattened = unbounded.text.replace('\n', "");
        assert!(
            flattened.contains("model.actors.A.properties.payload: {}"),
            "{}",
            unbounded.text
        );
        let serialized_len = match &model {
            merman_core::diagram::RenderSemanticModel::Sequence(sequence) => {
                serialize_bounded_json(
                    &sequence.compatibility_projection(&metadata.diagram_type),
                    unbounded_execution,
                )
                .unwrap_or_else(|_| panic!("sequence projection should serialize"))
                .len()
            }
            _ => unreachable!("fixture is a sequence diagram"),
        };
        let exact_bytes = serialized_len.max(unbounded.text.len());

        let exact_resources = crate::AsciiResourcePolicy::unbounded()
            .with_limit(AsciiResourceLimitId::MaxOutputBytes, exact_bytes)
            .expect("positive output-byte limit");
        let exact_execution = crate::operation::AsciiExecution::for_test(&exact_resources);
        build_semantic_fallback(
            &model,
            &metadata,
            AsciiExtent::new(80, 5),
            OutputBuildContext {
                color_mode: AsciiColorMode::Plain,
                profile: TerminalWidthProfile::Unicode,
                layout_profile: crate::AsciiLayoutProfile::Canonical,
                policy: AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
                execution: exact_execution,
            },
        )
        .expect("the exact sequence output boundary should succeed");

        let below_resources = crate::AsciiResourcePolicy::unbounded()
            .with_limit(AsciiResourceLimitId::MaxOutputBytes, exact_bytes - 1)
            .expect("positive output-byte limit");
        let below_execution = crate::operation::AsciiExecution::for_test(&below_resources);
        let error = build_semantic_fallback(
            &model,
            &metadata,
            AsciiExtent::new(80, 5),
            OutputBuildContext {
                color_mode: AsciiColorMode::Plain,
                profile: TerminalWidthProfile::Unicode,
                layout_profile: crate::AsciiLayoutProfile::Canonical,
                policy: AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
                execution: below_execution,
            },
        )
        .expect_err("the N-1 sequence output boundary should fail");

        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxOutputBytes
                    && details.actual == exact_bytes
                    && details.max == exact_bytes - 1
        ));
    }
fn sequence_semantic_preflight_enforces_json_nesting_before_serialization() {
        let mut nested = Value::Null;
        for _ in 0..300 {
            nested = Value::Array(vec![nested]);
        }
        let model = sequence_model_with_property(nested);
        let complexity = merman_core::resources::ModelComplexity::from_render_model(&model);

        let exact_policy = crate::AsciiResourcePolicy::unbounded()
            .with_limit(
                AsciiResourceLimitId::MaxNestingDepth,
                complexity.nesting_depth,
            )
            .expect("positive nesting limit");
        let exact_control = merman_core::OperationControl::new();
        let exact_execution = crate::operation::AsciiExecution::new(&exact_control, &exact_policy);
        preflight_semantic_model(&model, exact_execution)
            .expect("the exact JSON nesting boundary should pass");

        let below_policy = crate::AsciiResourcePolicy::unbounded()
            .with_limit(
                AsciiResourceLimitId::MaxNestingDepth,
                complexity.nesting_depth - 1,
            )
            .expect("positive nesting limit");
        let below_control = merman_core::OperationControl::new();
        let below_execution = crate::operation::AsciiExecution::new(&below_control, &below_policy);
        let error = preflight_semantic_model(&model, below_execution)
            .expect_err("the N-1 JSON nesting boundary should fail before serde");

        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxNestingDepth
                    && details.actual == complexity.nesting_depth
                    && details.max == complexity.nesting_depth - 1
        ));
    }
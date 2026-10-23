fn sequence_semantic_preflight_charges_json_items_as_layout_work() {
        let model = sequence_model_with_property(Value::Array(vec![Value::Null; 4_096]));
        let complexity = merman_core::resources::ModelComplexity::from_render_model(&model);
        assert!(complexity.items > 4_096);

        let exact_policy = crate::AsciiResourcePolicy::unbounded()
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, complexity.items)
            .expect("positive work limit");
        let exact_control = merman_core::OperationControl::new();
        let exact_execution = crate::operation::AsciiExecution::new(&exact_control, &exact_policy);
        preflight_semantic_model(&model, exact_execution)
            .expect("the exact JSON item work boundary should pass");

        let below_policy = crate::AsciiResourcePolicy::unbounded()
            .with_limit(
                AsciiResourceLimitId::MaxLayoutWorkUnits,
                complexity.items - 1,
            )
            .expect("positive work limit");
        let below_control = merman_core::OperationControl::new();
        let below_execution = crate::operation::AsciiExecution::new(&below_control, &below_policy);
        let error = preflight_semantic_model(&model, below_execution)
            .expect_err("the N-1 JSON item work boundary should fail before serde");

        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
                    && details.actual == complexity.items
                    && details.max == complexity.items - 1
        ));
    }
fn reconstructed_member_fields_are_preflighted_before_text_materialization() {
        const OVERSIZED_GRAPHEME: &str = "e\u{301}";
        let parsed =
            parsed_class_model("classDiagram\nclass Service {\n  +compute(input) Result\n}");
        for field in [
            "visibility",
            "id",
            "parameters",
            "return_type",
            "classifier",
        ] {
            let mut model = parsed.clone();
            let member = model
                .classes
                .get_mut("Service")
                .and_then(|class| class.methods.first_mut())
                .expect("Service method should exist");
            member.display_text.clear();
            match field {
                "visibility" => member.visibility = OVERSIZED_GRAPHEME.to_string(),
                "id" => member.id = OVERSIZED_GRAPHEME.to_string(),
                "parameters" => member.parameters = OVERSIZED_GRAPHEME.to_string(),
                "return_type" => member.return_type = OVERSIZED_GRAPHEME.to_string(),
                "classifier" => member.classifier = OVERSIZED_GRAPHEME.to_string(),
                _ => unreachable!(),
            }

            let exact_policy = AsciiResourcePolicy::default()
                .with_limit(
                    AsciiResourceLimitId::MaxGraphemeBytes,
                    OVERSIZED_GRAPHEME.len(),
                )
                .expect("grapheme resource limit should be valid");
            preflight_class_text(&model, &mut ResourceContext::new(exact_policy))
                .unwrap_or_else(|error| panic!("{field} must pass at the exact limit: {error:?}"));

            let policy = AsciiResourcePolicy::default()
                .with_limit(AsciiResourceLimitId::MaxGraphemeBytes, 2)
                .expect("grapheme resource limit should be valid");
            let mut resources = ResourceContext::new(policy);
            let error = match preflight_class_text(&model, &mut resources) {
                Ok(()) => panic!("{field} must be preflighted"),
                Err(error) => error,
            };
            let AsciiError::ResourceLimitExceeded(details) = error else {
                panic!("expected a grapheme resource error for {field}, got {error:?}");
            };
            assert_eq!(details.limit, AsciiResourceLimitId::MaxGraphemeBytes);
            assert_eq!(details.actual, OVERSIZED_GRAPHEME.len());
            assert_eq!(details.max, 2);
        }
    }
fn reconstructed_member_rejects_cross_field_grapheme_before_materializing() {
        let mut model =
            parsed_class_model("classDiagram\nclass Service {\n  +compute(input) Result\n}");
        let member = model
            .classes
            .get_mut("Service")
            .and_then(|class| class.methods.first_mut())
            .expect("Service method should exist");
        member.id = "compute".to_string();
        member.parameters = "\u{301}".to_string();
        member.return_type.clear();
        member.display_text.clear();

        let member = model
            .classes
            .get("Service")
            .and_then(|class| class.methods.first())
            .expect("Service method should remain available");
        let policy = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput)
            .with_limit(AsciiResourceLimitId::MaxGraphemeBytes, 2)
            .expect("grapheme resource limit should be valid");
        let resources = ResourceContext::new(policy);
        resources
            .charge_usage(3, 5)
            .expect("test checkpoint should fit");
        let materialized = Cell::new(false);
        let error = member_text_with_probe(member, &resources, || materialized.set(true))
            .expect_err("opening parenthesis plus combining mark exceeds two bytes");
        assert!(!materialized.get());
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxGraphemeBytes
                    && details.actual == 3
                    && details.max == 2
        ));
        assert_eq!(resources.layout_work_used(), 3);
        assert_eq!(resources.document_cells_used(), 5);
    }
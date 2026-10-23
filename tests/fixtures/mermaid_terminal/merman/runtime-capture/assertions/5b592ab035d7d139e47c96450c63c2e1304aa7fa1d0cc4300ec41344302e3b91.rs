fn displayed_member_classifier_is_preflighted_with_the_canonical_text_plan() {
        let mut model = parsed_class_model("classDiagram\nclass Service {\n  +value*\n}");
        let member = model
            .classes
            .get_mut("Service")
            .and_then(|class| class.members.first_mut())
            .expect("Service member should exist");
        member.display_text = "A".to_string();
        member.classifier = "\u{301}".to_string();

        let exact_policy = AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxGraphemeBytes, 3)
            .expect("grapheme resource limit should be valid");
        preflight_class_text(&model, &mut ResourceContext::new(exact_policy))
            .expect("display plus classifier should pass at the exact grapheme byte limit");

        let below_policy = AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxGraphemeBytes, 2)
            .expect("grapheme resource limit should be valid");
        let error = preflight_class_text(&model, &mut ResourceContext::new(below_policy))
            .expect_err("display plus classifier must be planned as one grapheme stream");
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxGraphemeBytes
                    && details.actual == 3
                    && details.max == 2
        ));
    }
fn class_sections_admit_exact_encoded_output_before_deferred_materialization() {
        let mut model = parsed_class_model("classDiagram\nclass Service");
        let class = model
            .classes
            .get_mut("Service")
            .expect("Service class should exist");
        class.text = "&lt;&amp;中".to_string();
        class.annotations = vec!["authored<&中".to_string()];
        class.members = vec![ClassMember {
            member_type: "attribute".to_string(),
            visibility: "+".to_string(),
            id: "value<&中".to_string(),
            classifier: "*".to_string(),
            parameters: String::new(),
            return_type: String::new(),
            display_text: String::new(),
            css_style: String::new(),
        }];
        let class = model
            .classes
            .get("Service")
            .expect("Service class should remain available");

        for mode in [
            AsciiColorMode::Plain,
            AsciiColorMode::Ansi16,
            AsciiColorMode::Ansi256,
            AsciiColorMode::TrueColor,
            AsciiColorMode::Html,
        ] {
            let base = AsciiRenderOptions::unicode().with_color_mode(mode);
            let base_policy = unbounded_policy();
            let measured_probe = Cell::new(false);
            let (measured, _, _) =
                render_class_section_fixture(class, &base, base_policy, &measured_probe);
            let expected = measured.expect("unbounded class section should render");
            assert!(measured_probe.get(), "mode={mode:?}");
            if mode == AsciiColorMode::Html {
                assert!(expected.contains("&lt;&amp;中"), "mode={mode:?}");
            } else {
                assert!(expected.contains("<&中"), "mode={mode:?}");
                assert!(expected.contains("+value<&中*"), "mode={mode:?}");
            }

            let exact_policy = base_policy
                .with_limit(AsciiResourceLimitId::MaxOutputBytes, expected.len())
                .expect("exact class output limit should be valid");
            let exact_probe = Cell::new(false);
            let (rendered, _, _) =
                render_class_section_fixture(class, &base, exact_policy, &exact_probe);
            assert_eq!(
                rendered.expect("exact class output should materialize"),
                expected,
                "mode={mode:?}"
            );
            assert!(exact_probe.get(), "mode={mode:?}");

            let below_policy = base_policy
                .with_limit(AsciiResourceLimitId::MaxOutputBytes, expected.len() - 1)
                .expect("max-minus-one class output limit should be valid");
            let below_probe = Cell::new(false);
            let (error, before, after) =
                render_class_section_fixture(class, &base, below_policy, &below_probe);
            assert!(!below_probe.get(), "mode={mode:?}");
            assert_eq!(after, before, "mode={mode:?}");
            assert!(
                matches!(
                    error.expect_err("max-minus-one class output must reject"),
                    AsciiError::ResourceLimitExceeded(details)
                        if details.limit == AsciiResourceLimitId::MaxOutputBytes
                            && details.actual == expected.len()
                            && details.max == expected.len() - 1
                ),
                "mode={mode:?}"
            );
        }
    }
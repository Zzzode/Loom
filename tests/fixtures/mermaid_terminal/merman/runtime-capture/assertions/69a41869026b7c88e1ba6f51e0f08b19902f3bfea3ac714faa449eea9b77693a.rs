fn class_summary_admits_exact_encoded_output_before_deferred_materialization() {
        let model = class_summary_model();
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
                render_class_summary_fixture(&model, &base, base_policy, &measured_probe);
            let expected = measured.expect("unbounded class summary should render");
            assert!(measured_probe.get(), "mode={mode:?}");
            assert!(expected.contains("relations:"), "mode={mode:?}");
            if mode == AsciiColorMode::Html {
                assert!(expected.contains("Source&lt;&amp;中"), "mode={mode:?}");
                assert!(expected.contains("self&lt;&amp;中"), "mode={mode:?}");
            } else {
                assert!(expected.contains("Source<&中"), "mode={mode:?}");
                assert!(expected.contains("self<&中"), "mode={mode:?}");
                assert!(expected.contains(
                    r#"endpoint1=[bytes=6 "a<&中", bytes=1 "b", bytes=32 "authored(bytes=11)=\"a<&中<br>b\""]"#,
                ));
            }

            let exact_policy = base_policy
                .with_limit(AsciiResourceLimitId::MaxOutputBytes, expected.len())
                .expect("exact class summary output limit should be valid");
            let exact_probe = Cell::new(false);
            let (rendered, _, _) =
                render_class_summary_fixture(&model, &base, exact_policy, &exact_probe);
            assert_eq!(
                rendered.expect("exact class summary should materialize"),
                expected,
                "mode={mode:?}"
            );
            assert!(exact_probe.get(), "mode={mode:?}");

            let below_policy = base_policy
                .with_limit(AsciiResourceLimitId::MaxOutputBytes, expected.len() - 1)
                .expect("max-minus-one class summary limit should be valid");
            let below_probe = Cell::new(false);
            let (error, before, after) =
                render_class_summary_fixture(&model, &base, below_policy, &below_probe);
            assert!(!below_probe.get(), "mode={mode:?}");
            assert_eq!(after, before, "mode={mode:?}");
            assert!(matches!(
                error.expect_err("max-minus-one class summary must reject"),
                AsciiError::ResourceLimitExceeded(details)
                    if details.limit == AsciiResourceLimitId::MaxOutputBytes
                        && details.actual == expected.len()
                        && details.max == expected.len() - 1
            ));
        }
    }
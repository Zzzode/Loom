fn flowchart_and_sequence_width_policy_matrix_is_typed_and_complete() {
    let cases = [
        (
            "flowchart/issue_53_long_node_labels.mmd",
            &[
                "browser / agent",
                "CreateInvalidation",
                "reserve / check owner",
            ][..],
        ),
        (
            "sequence/self_messages_with_notes.mmd",
            &[
                "Main Process",
                "event.preventDefault()",
                "WINDOW_CLOSE_REQUESTED",
                "window.destroy()",
            ][..],
        ),
    ];

    for (fixture, required_fields) in cases {
        let model = parse_model(&local_semantic_input(fixture));
        for layout_profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
            for base_options in option_matrix() {
                let plain_options = base_options
                    .with_layout_profile(layout_profile)
                    .with_color_mode(AsciiColorMode::Plain);
                let baseline =
                    render_model_report(&model, &plain_options, AsciiViewportPolicy::default())
                        .unwrap_or_else(|error| {
                            panic!(
                                "baseline {fixture} render failed for {plain_options:?}: {error}"
                            )
                        });

                assert_sequence_message_lifelines(
                    &model,
                    &baseline.text,
                    plain_options.terminal_width_profile,
                );
                for max_width in WIDTH_MATRIX {
                    let overflowed = baseline.primary_extent.width > max_width;
                    for (color_mode, encoding) in [
                        (AsciiColorMode::Plain, AsciiOutputEncoding::Plain),
                        (AsciiColorMode::Ansi16, AsciiOutputEncoding::Ansi16),
                    ] {
                        let options = plain_options.with_color_mode(color_mode);
                        let allow = render_model_report(
                            &model,
                            &options,
                            AsciiViewportPolicy::with_max_width(max_width)
                                .overflow(OverflowPolicy::Allow),
                        )
                        .unwrap_or_else(|error| {
                            panic!(
                                "Allow {fixture} render failed for {options:?}, width={max_width}: {error}"
                            )
                        });
                        assert_eq!(allow.encoding, encoding);
                        assert_eq!(allow.primary_extent, baseline.primary_extent);
                        assert_eq!(allow.emitted_extent, baseline.emitted_extent);
                        assert_eq!(allow.overflowed, overflowed);
                        assert_eq!(
                            allow.outcome,
                            if overflowed {
                                AsciiOutputOutcome::WideAllowed
                            } else {
                                AsciiOutputOutcome::Primary
                            }
                        );
                        assert_eq!(
                            plain_text_for_encoding(&allow.text, encoding),
                            baseline.text
                        );

                        let error_result = render_model_report(
                            &model,
                            &options,
                            AsciiViewportPolicy::with_max_width(max_width)
                                .overflow(OverflowPolicy::Error),
                        );
                        if overflowed {
                            assert!(
                                matches!(
                                    error_result,
                                    Err(AsciiError::WidthOverflow {
                                        max_width: actual_max,
                                        actual_width,
                                        ..
                                    }) if actual_max == max_width
                                        && actual_width == baseline.primary_extent.width
                                ),
                                "Error {fixture}/{options:?}/width={max_width} produced {error_result:?}"
                            );
                        } else {
                            let report = error_result.unwrap_or_else(|error| {
                                panic!(
                                    "fitting Error {fixture}/{options:?}/width={max_width} failed: {error}"
                                )
                            });
                            assert_eq!(report.outcome, AsciiOutputOutcome::Primary);
                            assert_eq!(report.encoding, encoding);
                            assert_eq!(report.emitted_extent, baseline.emitted_extent);
                        }
                    }

                    let fallback = render_model_report(
                        &model,
                        &plain_options,
                        AsciiViewportPolicy::with_max_width(max_width)
                            .overflow(OverflowPolicy::Fallback),
                    )
                    .unwrap_or_else(|error| {
                        panic!(
                            "Fallback {fixture} render failed for {plain_options:?}, width={max_width}: {error}"
                        )
                    });
                    assert_eq!(fallback.encoding, AsciiOutputEncoding::Plain);
                    assert_eq!(fallback.primary_extent, baseline.primary_extent);
                    if overflowed {
                        assert_eq!(fallback.outcome, AsciiOutputOutcome::Fallback);
                        assert_eq!(fallback.projection, AsciiProjection::StructuredText);
                        assert!(fallback.fallback.attempted);
                        assert!(fallback.emitted_extent.width <= max_width);
                        let collapsed = collapsed_authored_text(&fallback.text);
                        for field in required_fields {
                            assert!(
                                collapsed.contains(&collapsed_authored_text(field)),
                                "fallback {fixture}/width={max_width} lost {field:?}:\n{}",
                                fallback.text
                            );
                        }
                    } else {
                        assert_eq!(fallback.outcome, AsciiOutputOutcome::Primary);
                        assert!(!fallback.fallback.attempted);
                        assert_eq!(fallback.text, baseline.text);
                    }
                }

                for (color_mode, _) in ENCODING_MATRIX.into_iter().skip(1) {
                    let styled = plain_options.with_color_mode(color_mode);
                    let error = render_model_report(
                        &model,
                        &styled,
                        AsciiViewportPolicy::with_max_width(WIDTH_MATRIX[0])
                            .overflow(OverflowPolicy::Fallback),
                    )
                    .expect_err("styled fallback must fail capability preflight");
                    assert_eq!(
                        error,
                        AsciiError::InvalidOption {
                            field: "ascii_viewport.overflow",
                            message: "fallback is not admitted for the selected output encoding",
                        }
                    );
                }
            }
        }
    }
}
fn flowchart_and_sequence_encoding_matrix_preserves_plain_geometry() {
    let cases = [
        (
            "flowchart/issue_53_long_node_labels.mmd",
            &[
                "browser / agent",
                "CreateInvalidation",
                "DynamoDB reservations",
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
                let plain =
                    render_model_report(&model, &plain_options, AsciiViewportPolicy::default())
                        .unwrap_or_else(|error| {
                            panic!("plain {fixture} render failed for {plain_options:?}: {error}")
                        });

                assert_sequence_message_lifelines(
                    &model,
                    &plain.text,
                    plain_options.terminal_width_profile,
                );
                for field in required_fields {
                    assert!(
                        plain.text.contains(field),
                        "plain {fixture} lost authored field {field:?}:\n{}",
                        plain.text
                    );
                }

                for (color_mode, encoding) in ENCODING_MATRIX {
                    let options = plain_options.with_color_mode(color_mode);
                    let report =
                        render_model_report(&model, &options, AsciiViewportPolicy::default())
                            .unwrap_or_else(|error| {
                                panic!(
                                    "{encoding:?} {fixture} render failed for {options:?}: {error}"
                                )
                            });

                    assert_eq!(report.encoding, encoding, "{fixture}/{options:?}");
                    assert_eq!(
                        report.primary_extent, plain.primary_extent,
                        "{fixture}/{options:?} primary extent drifted"
                    );
                    assert_eq!(
                        report.emitted_extent, plain.emitted_extent,
                        "{fixture}/{options:?} emitted extent drifted"
                    );
                    assert_eq!(
                        plain_text_for_encoding(&report.text, encoding),
                        plain.text,
                        "{fixture}/{options:?} styled bytes changed semantic text"
                    );
                }
            }
        }
    }
}
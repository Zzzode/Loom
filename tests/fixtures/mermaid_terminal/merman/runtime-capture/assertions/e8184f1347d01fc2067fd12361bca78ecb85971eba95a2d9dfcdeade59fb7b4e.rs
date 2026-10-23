fn representative_finalizers_expose_stable_extents_and_semantic_fields() {
    struct Case {
        name: &'static str,
        source: String,
        required_fields: &'static [&'static str],
        rectangular: bool,
        expected_extent: (usize, usize),
    }

    let cases = [
        Case {
            name: "sequence",
            source: local_semantic_input("sequence/dense_control_rows.mmd"),
            required_fields: &[
                "Start",
                "Coordinate",
                "Parallel Branches",
                "Fallback",
                "Retry",
                "Stop",
            ],
            rectangular: false,
            expected_extent: (39, 30),
        },
        Case {
            name: "class",
            source: local_semantic_input("class/dense_multiline_relations.mmd"),
            required_fields: &[
                "Gateway",
                "Service",
                "receives",
                "request",
                "invalidates",
                "entry",
            ],
            rectangular: false,
            expected_extent: (91, 41),
        },
        Case {
            name: "er",
            source: local_semantic_input("er/dense_multiline_relations.mmd"),
            required_fields: &["CUSTOMER", "ORDER", "places", "orders", "settles", "order"],
            rectangular: false,
            expected_extent: (97, 41),
        },
        Case {
            name: "xychart",
            source: local_semantic_input("xychart/horizontal_mixed_cjk.mmd"),
            required_fields: &["营收", "北区", "南区", "分数", "4"],
            rectangular: false,
            expected_extent: (270, 10),
        },
        Case {
            name: "structured-text",
            source: "timeline\ntitle Basic\n2024 : Event".to_string(),
            required_fields: &["Basic", "Event"],
            rectangular: false,
            expected_extent: (28, 4),
        },
    ];

    for case in cases {
        let model = parse_model(&case.source);
        for (charset_name, options) in [
            ("ascii", AsciiRenderOptions::ascii()),
            ("unicode", AsciiRenderOptions::unicode()),
        ] {
            let rendered = render_model(&model, &options).unwrap_or_else(|error| {
                panic!(
                    "{} ({charset_name}) characterization render failed: {error}",
                    case.name
                )
            });
            let (width, height) = terminal_extent(&rendered);
            assert_eq!(
                (width, height),
                case.expected_extent,
                "{} ({charset_name}) extent changed",
                case.name
            );

            assert!(
                width > 0,
                "{} ({charset_name}) width must be positive",
                case.name
            );
            assert!(
                height > 0,
                "{} ({charset_name}) height must be positive",
                case.name
            );
            for field in case.required_fields {
                assert!(
                    rendered.contains(field),
                    "{} ({charset_name}) lost authored field {field:?}:\n{rendered}",
                    case.name
                );
            }
            if case.rectangular {
                assert_rectangular_terminal_grid(&rendered);
            }

            let fits = WIDTH_MATRIX.map(|max_width| width <= max_width);
            assert!(
                fits.windows(2).all(|window| !window[0] || window[1]),
                "{} extent-fit classification must be monotonic: width={width}, fits={fits:?}",
                case.name
            );
        }
    }
}
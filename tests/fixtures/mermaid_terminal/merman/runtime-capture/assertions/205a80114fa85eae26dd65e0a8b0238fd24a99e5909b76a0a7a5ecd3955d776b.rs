fn compact_sequence_corpus_preserves_lifecycle_controls_and_terminal_extents() {
    struct Case {
        fixture: &'static str,
        required_fields: &'static [&'static str],
        canonical_extent: (usize, usize),
        compact_extent: (usize, usize),
    }

    let cases = [
        Case {
            fixture: "sequence/dense_control_rows.mmd",
            required_fields: &[
                "Start",
                "Coordinate",
                "Parallel Branches",
                "Fallback",
                "Ship",
                "Return",
                "Retry",
                "Stop",
            ],
            canonical_extent: (39, 30),
            compact_extent: (37, 30),
        },
        Case {
            fixture: "sequence/multiple_messages.mmd",
            required_fields: &[
                "Alice", "Bob", "Charlie", "Hello", "Forward", "Reply", "Done",
            ],
            canonical_extent: (37, 16),
            compact_extent: (33, 16),
        },
        Case {
            fixture: "sequence/self_messages_with_notes.mmd",
            required_fields: &[
                "User",
                "Main Process",
                "Renderer",
                "3s Fallback Timer",
                "event.preventDefault()",
                "WINDOW_CLOSE_REQUESTED",
                "Multiple panels",
                "Single panel",
                "Panel removed",
                "window.destroy()",
                "Panel reopens",
            ],
            canonical_extent: (107, 58),
            compact_extent: (105, 58),
        },
    ];
    let profiles = [
        AsciiRenderOptions::ascii(),
        AsciiRenderOptions::unicode(),
        AsciiRenderOptions::ascii().with_terminal_width_profile(TerminalWidthProfile::Cjk),
        AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Cjk),
    ];

    for case in cases {
        let model = parse_model(&local_semantic_input(case.fixture));
        for options in profiles {
            let canonical = render_model(&model, &options).unwrap_or_else(|error| {
                panic!(
                    "canonical {} render failed for {options:?}: {error}",
                    case.fixture
                )
            });
            let compact_options = options.with_layout_profile(AsciiLayoutProfile::Compact);
            let compact = render_model(&model, &compact_options).unwrap_or_else(|error| {
                panic!(
                    "compact {} render failed for {compact_options:?}: {error}",
                    case.fixture
                )
            });

            let canonical_extent =
                terminal_extent_with_profile(&canonical, options.terminal_width_profile);
            let compact_extent =
                terminal_extent_with_profile(&compact, compact_options.terminal_width_profile);
            assert_eq!(
                canonical_extent, case.canonical_extent,
                "canonical {} extent changed for {options:?}",
                case.fixture
            );
            assert_eq!(
                compact_extent, case.compact_extent,
                "compact {} extent changed for {compact_options:?}",
                case.fixture
            );
            assert!(compact_extent.0 <= canonical_extent.0, "{}", case.fixture);
            assert_eq!(compact_extent.1, canonical_extent.1, "{}", case.fixture);
            assert!(
                compact_extent.0 * compact_extent.1 <= canonical_extent.0 * canonical_extent.1,
                "{}",
                case.fixture
            );

            assert_sequence_message_lifelines(&model, &canonical, options.terminal_width_profile);
            assert_sequence_message_lifelines(
                &model,
                &compact,
                compact_options.terminal_width_profile,
            );
            for field in case.required_fields {
                assert!(
                    canonical.contains(field),
                    "canonical {} lost authored field {field:?}:\n{canonical}",
                    case.fixture
                );
                assert!(
                    compact.contains(field),
                    "compact {} lost authored field {field:?}:\n{compact}",
                    case.fixture
                );
            }
        }
    }
}
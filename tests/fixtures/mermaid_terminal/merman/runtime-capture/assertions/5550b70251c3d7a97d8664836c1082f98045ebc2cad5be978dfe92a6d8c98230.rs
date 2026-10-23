fn sequence_rect_par_over_blocks_are_core_control_signals() {
    struct Case {
        name: &'static str,
        input: &'static str,
        signals: &'static [(i32, &'static str)],
    }

    let cases = [
        Case {
            name: "rect",
            input: "sequenceDiagram\nparticipant A\nparticipant B\nrect rgba(0,0,0,0.1)\nA->>B: Shaded\nend",
            signals: &[
                (LINETYPE_RECT_START, "rgba(0,0,0,0.1)"),
                (LINETYPE_RECT_END, ""),
            ],
        },
        Case {
            name: "par_over",
            input: "sequenceDiagram\nparticipant A\nparticipant B\npar_over Everyone\nA->>B: Work\nend",
            signals: &[
                (LINETYPE_PAR_OVER_START, "Everyone"),
                (LINETYPE_PAR_END, ""),
            ],
        },
    ];

    for case in cases {
        let model = parse_sequence_render_model(case.input);
        let control_messages = model
            .messages
            .iter()
            .filter(|message| message.from.is_none() && message.to.is_none())
            .collect::<Vec<_>>();

        assert_eq!(
            control_messages.len(),
            case.signals.len(),
            "{} should have expected control marker count",
            case.name
        );

        let actual = control_messages
            .iter()
            .map(|message| (message.message_type, message.message_text()))
            .collect::<Vec<_>>();
        assert_eq!(
            actual, case.signals,
            "{} should preserve core control line types and labels",
            case.name
        );
        assert!(
            model
                .messages
                .iter()
                .any(|message| message.from.is_some() && message.to.is_some()),
            "{} should still include drawable messages inside the block",
            case.name
        );
    }
}
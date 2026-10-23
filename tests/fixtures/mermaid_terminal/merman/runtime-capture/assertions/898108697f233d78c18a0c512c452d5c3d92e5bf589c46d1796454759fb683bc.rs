fn sequence_control_blocks_are_core_control_signals() {
    struct Case {
        name: &'static str,
        input: &'static str,
        signals: &'static [(i32, &'static str)],
    }

    let cases = [
        Case {
            name: "loop",
            input: "sequenceDiagram\nparticipant A\nparticipant B\nloop Every minute\nA->>B: Ping\nend",
            signals: &[
                (LINETYPE_LOOP_START, "Every minute"),
                (LINETYPE_LOOP_END, ""),
            ],
        },
        Case {
            name: "opt",
            input: "sequenceDiagram\nparticipant A\nparticipant B\nopt A is ready\nA->>B: Send\nend",
            signals: &[(LINETYPE_OPT_START, "A is ready"), (LINETYPE_OPT_END, "")],
        },
        Case {
            name: "break",
            input: "sequenceDiagram\nparticipant A\nparticipant B\nbreak Failure\nA->>B: Stop\nend",
            signals: &[(LINETYPE_BREAK_START, "Failure"), (LINETYPE_BREAK_END, "")],
        },
        Case {
            name: "alt",
            input: "sequenceDiagram\nparticipant A\nparticipant B\nalt Success\nA->>B: OK\nelse Failure\nB-->>A: Retry\nend",
            signals: &[
                (LINETYPE_ALT_START, "Success"),
                (LINETYPE_ALT_ELSE, "Failure"),
                (LINETYPE_ALT_END, ""),
            ],
        },
        Case {
            name: "par",
            input: "sequenceDiagram\nparticipant A\nparticipant B\npar First\nA->>B: One\nand Second\nB-->>A: Two\nend",
            signals: &[
                (LINETYPE_PAR_START, "First"),
                (LINETYPE_PAR_AND, "Second"),
                (LINETYPE_PAR_END, ""),
            ],
        },
        Case {
            name: "critical",
            input: "sequenceDiagram\nparticipant A\nparticipant B\ncritical Must lock\nA->>B: Lock\noption Timeout\nB-->>A: Backoff\nend",
            signals: &[
                (LINETYPE_CRITICAL_START, "Must lock"),
                (LINETYPE_CRITICAL_OPTION, "Timeout"),
                (LINETYPE_CRITICAL_END, ""),
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
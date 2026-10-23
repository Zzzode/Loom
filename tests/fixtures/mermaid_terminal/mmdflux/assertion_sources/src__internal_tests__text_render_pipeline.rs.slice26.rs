#[test]
    fn state_composite_retains_resume_terminal_arrowhead() {
        let text = render_state_fixture("composite.mmd");
        let count = text.matches('▲').count();
        assert_eq!(
            count, 1,
            "state/composite must retain the `▲` arrowhead terminal on the resume edge; got {count} occurrences in:\n{text}"
        );
    }
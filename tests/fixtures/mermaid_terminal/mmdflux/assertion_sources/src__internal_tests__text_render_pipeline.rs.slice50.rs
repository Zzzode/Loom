#[test]
    fn text_five_fan_in_lr_does_not_leave_a_dangling_bottom_stem() {
        let output = render_flowchart_fixture("five_fan_in_lr.mmd");

        assert_eq!(
            output.lines().last(),
            Some("└───┘"),
            "five_fan_in_lr should end at E's bottom border instead of leaving a dangling bottom stem\n{output}"
        );
        assert_eq!(
            output.chars().filter(|&ch| ch == '▲').count(),
            0,
            "five_fan_in_lr should not need upward arrowheads when the LR fan-in stays on the compact text route\n{output}"
        );
    }
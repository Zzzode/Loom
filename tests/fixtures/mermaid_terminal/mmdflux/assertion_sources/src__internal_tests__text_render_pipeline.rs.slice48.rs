#[test]
    fn text_five_fan_out_lr_does_not_gain_extra_terminal_tail_rows() {
        let output = render_flowchart_fixture("five_fan_out_lr.mmd");
        let lines: Vec<&str> = output.lines().collect();
        let last_non_empty = lines
            .iter()
            .rposition(|line| !line.trim().is_empty())
            .expect("five_fan_out_lr should render at least one row");
        let target_e_bottom = lines
            .iter()
            .rposition(|line| line.contains("└──────────┘"))
            .expect("five_fan_out_lr should render a bottom border for Target E");

        assert_eq!(
            last_non_empty, target_e_bottom,
            "five_fan_out_lr should end at Target E's bottom border instead of leaving a dangling tail row\n{output}"
        );
    }
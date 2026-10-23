#[test]
    fn text_five_fan_out_lr_keeps_direct_fanout_channels_for_inner_targets() {
        let output = render_flowchart_fixture("five_fan_out_lr.mmd");
        let forward_arrowheads = output.chars().filter(|&ch| ch == '►').count();
        let upward_arrowheads = output.chars().filter(|&ch| ch == '▲').count();

        assert_eq!(
            forward_arrowheads, 5,
            "five_fan_out_lr should keep one forward arrowhead per target instead of detouring inner branches through sibling columns\n{output}"
        );
        assert_eq!(
            upward_arrowheads, 0,
            "five_fan_out_lr should not need upward arrowheads when LR fan-out branches stay on their direct channels\n{output}"
        );
    }
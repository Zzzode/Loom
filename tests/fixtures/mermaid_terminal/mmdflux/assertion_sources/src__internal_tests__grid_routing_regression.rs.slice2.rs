#[test]
    fn fan_in_arrival_points_remain_spread_after_shared_attachment_planner() {
        let output = render_flowchart_fixture("five_fan_in.mmd");
        assert!(output.contains("A"));
        assert!(output.contains("Target"));
        assert_distinct_arrival_x("five_fan_in.mmd", "F");
    }
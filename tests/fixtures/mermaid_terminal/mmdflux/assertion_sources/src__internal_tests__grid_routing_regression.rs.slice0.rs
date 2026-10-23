#[test]
    fn fan_in_no_adjacent_arrows() {
        let output = render_flowchart_fixture("fan_in.mmd");
        assert_no_adjacent_arrows(&output, "fan_in.mmd");
    }
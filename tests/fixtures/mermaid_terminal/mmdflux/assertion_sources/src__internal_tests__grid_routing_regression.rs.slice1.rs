#[test]
    fn fan_out_no_adjacent_arrows() {
        let output = render_flowchart_fixture("fan_out.mmd");
        assert_no_adjacent_arrows(&output, "fan_out.mmd");
    }
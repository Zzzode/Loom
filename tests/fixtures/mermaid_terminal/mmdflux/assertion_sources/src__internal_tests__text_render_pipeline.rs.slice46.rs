#[test]
    fn simple_cycle_compact_backward_routing() {
        let output = render_flowchart_fixture("simple_cycle.mmd");
        let line_count = output.lines().count();
        assert!(
            line_count < 30,
            "simple_cycle.mmd should be compact, got {line_count} lines"
        );
    }
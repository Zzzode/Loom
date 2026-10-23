#[test]
    fn multiple_cycles_compact_backward_routing() {
        let output = render_flowchart_fixture("multiple_cycles.mmd");
        let line_count = output.lines().count();
        assert!(
            line_count < 40,
            "multiple_cycles.mmd should be compact, got {line_count} lines"
        );
    }
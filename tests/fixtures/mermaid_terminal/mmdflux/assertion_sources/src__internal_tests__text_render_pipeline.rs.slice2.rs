#[test]
    fn decision_renders_diamond() {
        let output = render_flowchart_fixture("decision.mmd");
        assert!(output.contains("Is it working?"));
        assert!(output.contains('<') || output.contains('>'));
    }
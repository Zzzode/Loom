#[test]
    fn complex_renders_without_panic() {
        let output = render_flowchart_fixture("complex.mmd");
        assert!(!output.is_empty());
        assert!(output.contains("Input"));
        assert!(output.contains("Output"));
    }
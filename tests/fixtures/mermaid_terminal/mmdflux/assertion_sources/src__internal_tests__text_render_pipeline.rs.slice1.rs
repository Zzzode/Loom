#[test]
    fn simple_renders() {
        let output = render_flowchart_fixture("simple.mmd");
        assert!(!output.is_empty());
        assert!(output.contains("Start"));
        assert!(output.contains("End"));
    }
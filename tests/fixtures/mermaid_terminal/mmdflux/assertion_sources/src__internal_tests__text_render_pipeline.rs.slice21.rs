#[test]
    fn ci_pipeline_renders() {
        let output = render_flowchart_fixture("ci_pipeline.mmd");
        assert!(output.contains("Build"));
        assert!(output.contains("Test"));
        assert!(output.contains("Deploy?"));
    }
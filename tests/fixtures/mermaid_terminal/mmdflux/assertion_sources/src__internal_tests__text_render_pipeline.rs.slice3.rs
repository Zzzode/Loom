#[test]
    fn shapes_render_distinctly() {
        let output = render_flowchart_fixture("shapes.mmd");
        assert!(output.contains("Rectangle Node"));
        assert!(output.contains("Rounded Node"));
        assert!(output.contains("Diamond Node"));
    }
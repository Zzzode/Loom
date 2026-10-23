#[test]
    fn edge_styles_render() {
        let output = render_flowchart_fixture("edge_styles.mmd");
        assert!(output.contains("Solid"));
        assert!(output.contains("Dotted"));
        assert!(output.contains("Thick"));
    }
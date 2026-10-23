#[test]
    fn shapes_junction_ascii_degrades() {
        let output = render_flowchart_fixture_ascii("shapes_junction.mmd");
        assert!(output.contains("o"));
        assert!(output.contains("(o)"));
        assert!(output.contains("x"));
    }
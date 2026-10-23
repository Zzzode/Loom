#[test]
    fn shapes_junction_render_glyphs() {
        let output = render_flowchart_fixture("shapes_junction.mmd");
        assert!(output.contains('●'));
        assert!(output.contains('◉'));
        assert!(output.contains('⊗'));
    }
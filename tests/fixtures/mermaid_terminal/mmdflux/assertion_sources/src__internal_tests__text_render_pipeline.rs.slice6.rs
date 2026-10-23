#[test]
    fn shapes_special_render_bar_and_text() {
        let output = render_flowchart_fixture("shapes_special.mmd");
        assert!(
            output.contains('┃'),
            "Fork/join in LR should use heavy vertical bar"
        );
        assert!(output.contains("Note"));
    }
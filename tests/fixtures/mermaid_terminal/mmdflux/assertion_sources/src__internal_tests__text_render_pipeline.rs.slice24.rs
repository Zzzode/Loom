#[test]
    fn render_simple_diagram_unchanged() {
        let output = crate::render_diagram(
            "graph TD\nA --> B\n",
            OutputFormat::Text,
            &RenderConfig::default(),
        )
        .unwrap();

        assert!(
            output.contains('A'),
            "output should contain node A: {output}"
        );
        assert!(
            output.contains('B'),
            "output should contain node B: {output}"
        );
    }
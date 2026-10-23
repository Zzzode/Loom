#[test]
    fn render_with_subgraph_produces_borders() {
        let output = crate::render_diagram(
            "graph TD\nsubgraph sg1[Group]\nA --> B\nend\n",
            OutputFormat::Text,
            &RenderConfig::default(),
        )
        .unwrap();

        assert!(
            output.contains('\u{250c}') || output.contains('+'),
            "output should contain top-left corner: {output}"
        );
        assert!(
            output.contains('\u{2518}') || output.contains('+'),
            "output should contain bottom-right corner: {output}"
        );
        assert!(
            output.contains("Group"),
            "output should contain title: {output}"
        );
    }
#[test]
    fn state_final_terminal_retains_prior_arrowhead() {
        let input = "stateDiagram-v2\n    A --> B\n    B --> [*]";
        let text = crate::render_diagram(input, OutputFormat::Text, &RenderConfig::default())
            .expect("Failed to render inline state diagram");

        assert!(
            text.contains('◉'),
            "Expected final state marker in output:\n{text}"
        );
        let arrow_index = text
            .find('▼')
            .expect("Expected at least one downward arrow in output");
        let point_index = text
            .find('◉')
            .expect("Expected final state marker in output");
        assert!(
            arrow_index < point_index,
            "Expected a downward arrow before the final state marker in output:\n{text}"
        );
    }
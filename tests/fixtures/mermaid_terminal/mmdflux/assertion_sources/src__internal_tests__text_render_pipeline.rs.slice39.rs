#[test]
    fn long_label_renders_without_panic() {
        let output = render_flowchart_input(
            "graph TD\n    A -->|this is a very long label that might overflow| B",
        );
        assert!(!output.is_empty());
        assert!(output.contains(" A "), "Node A should render:\n{output}");
        assert!(output.contains(" B "), "Node B should render:\n{output}");
    }
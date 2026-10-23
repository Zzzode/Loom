#[test]
    fn labeled_edge_lr_direction() {
        let output = render_flowchart_input("graph LR\n    A -->|label| B");
        assert!(output.contains(" A "), "Should contain node A:\n{output}");
        assert!(output.contains(" B "), "Should contain node B:\n{output}");
        assert!(
            output.contains("label"),
            "Expected 'label' in LR layout:\n{output}"
        );
    }
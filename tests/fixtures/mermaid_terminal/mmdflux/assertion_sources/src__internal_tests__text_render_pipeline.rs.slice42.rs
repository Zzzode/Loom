#[test]
    fn mixed_labeled_and_unlabeled() {
        let output = render_flowchart_input(
            "graph TD\n    A -->|yes| B\n    A --> C\n    B --> D\n    C -->|error| D",
        );
        assert!(output.contains("yes"), "Expected 'yes' label:\n{output}");
        assert!(
            output.contains("error"),
            "Expected 'error' label:\n{output}"
        );
        for node in ["A", "B", "C", "D"] {
            assert!(
                output.contains(&format!(" {node} ")),
                "Expected node {node}:\n{output}"
            );
        }
    }
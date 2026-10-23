#[test]
    fn fan_out_with_labels() {
        let output = render_flowchart_input(
            "graph TD\n    A -->|yes| B\n    A -->|no| C\n    A -->|maybe| D",
        );
        assert!(output.contains("yes"), "Expected 'yes' label:\n{output}");
        assert!(output.contains("no"), "Expected 'no' label:\n{output}");
        assert!(
            output.contains("maybe"),
            "Expected 'maybe' label:\n{output}"
        );
    }
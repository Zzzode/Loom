#[test]
    fn all_edges_labeled() {
        let output = render_flowchart_input(
            "graph TD\n    A -->|start| B\n    B -->|process| C\n    C -->|end| D",
        );
        assert!(output.contains("end"), "Expected 'end' label:\n{output}");
        assert!(output.contains(" A "), "Expected node A:\n{output}");
        assert!(output.contains(" B "), "Expected node B:\n{output}");
        assert!(output.contains(" D "), "Expected node D:\n{output}");
        assert!(
            output.contains("┌───┐"),
            "Expected at least one node box:\n{output}"
        );
    }
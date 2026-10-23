#[test]
    fn labeled_edges_show_labels() {
        let output = render_flowchart_fixture("labeled_edges.mmd");
        assert!(output.contains("initialize") || output.contains("configure"));
        assert!(
            output.contains("yes"),
            "Expected 'yes' label in output:\n{output}"
        );
        assert!(
            output.contains("no"),
            "Expected 'no' label in output:\n{output}"
        );
    }
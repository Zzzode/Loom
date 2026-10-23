#[test]
    fn labeled_edges_reasonable_height() {
        let output = render_flowchart_fixture("labeled_edges.mmd");
        let line_count = output.lines().count();

        assert!(
            line_count < 40,
            "labeled_edges.mmd should render in under 40 lines, got {line_count}"
        );

        for label in &["initialize", "configure", "yes", "no", "retry"] {
            assert!(
                output.contains(label),
                "Output should contain label '{label}'"
            );
        }
    }
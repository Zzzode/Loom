#[test]
    fn ascii_issue_21_backward_edge_does_not_clip_right_edge() {
        let output = render_flowchart_fixture_ascii("callgraph_feedback_cycle.mmd");

        let clipped_lines: Vec<&str> = output
            .lines()
            .filter(|line| line.trim_end().ends_with('-'))
            .collect();
        assert!(
            clipped_lines.is_empty(),
            "ASCII output should not be clipped on the right edge for issue #21.\nFound clipped lines:\n{}\n\nFull output:\n{}",
            clipped_lines.join("\n"),
            output
        );
    }
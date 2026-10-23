#[test]
    fn branching_labels_do_not_overlap() {
        let output = render_flowchart_fixture("label_spacing.mmd");

        assert!(output.contains("valid"), "Should contain 'valid' label");
        assert!(output.contains("invalid"), "Should contain 'invalid' label");
        assert!(
            !output.contains("valinvalid"),
            "Labels should not merge into 'valinvalid'"
        );
        assert!(
            !output.contains("invalidvalid"),
            "Labels should not merge into 'invalidvalid'"
        );

        let lines: Vec<&str> = output.lines().collect();
        let a_line = lines.iter().position(|line| line.contains(" A ")).unwrap();
        let b_line = lines.iter().rposition(|line| line.contains(" B ")).unwrap();
        let label_line = lines
            .iter()
            .position(|line| line.contains("valid"))
            .unwrap();
        assert!(
            label_line > a_line && label_line < b_line,
            "Label at line {} should be between A (line {}) and B (line {})\n{}",
            label_line,
            a_line,
            b_line,
            output
        );
    }
#[test]
    fn git_workflow_td_backward_label_rejects_stale_authoritative_center() {
        let text = render_flowchart_fixture("git_workflow_td.mmd");
        assert_eq!(
            text.matches("git pull").count(),
            1,
            "git pull label should appear exactly once. Output:\n{text}"
        );
        let row = text.lines().position(|l| l.contains("git pull")).unwrap();
        let line = text.lines().nth(row).unwrap();
        let label_col = char_col_of(line, "git pull").expect("git pull col should be locatable");
        let pipe_col = line
            .chars()
            .enumerate()
            .filter_map(|(i, c)| (c == '│').then_some(i))
            .next();
        if let Some(pipe_col) = pipe_col {
            let drift = (label_col as isize - pipe_col as isize).abs();
            assert!(
                drift > 3,
                "git pull should reject the stale authoritative center and use the safe Pass-3 midpoint; got drift {drift}. Output:\n{text}"
            );
        }
    }
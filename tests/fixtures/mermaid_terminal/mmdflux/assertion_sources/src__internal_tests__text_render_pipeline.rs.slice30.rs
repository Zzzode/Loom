#[test]
    fn state_transitions_retry_label_stays_corridor_adjacent() {
        let text = render_state_fixture("transitions.mmd");
        let row_idx = text
            .lines()
            .position(|l| l.contains("retry"))
            .expect("retry should appear");
        let retry_col = char_col_of(text.lines().nth(row_idx).unwrap(), "retry")
            .expect("retry col should be locatable");

        let neighboring_rows = [row_idx.saturating_sub(1), row_idx + 1];
        let nearest_pipe_col = neighboring_rows
            .iter()
            .filter_map(|&r| text.lines().nth(r))
            .filter_map(|line| {
                line.chars()
                    .enumerate()
                    .filter_map(|(i, c)| (c == '│').then_some(i))
                    .last()
            })
            .next()
            .expect("neighboring row should carry a corridor `│`");

        let drift = (retry_col as isize - nearest_pipe_col as isize).abs();
        assert!(
            drift <= 2,
            "state/transitions `retry` label at col {retry_col} must sit within 2 cells of the corridor `│` at col {nearest_pipe_col}; got {drift} cells. Output:\n{text}"
        );
    }
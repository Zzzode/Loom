#[test]
    fn flowchart_cjk_subgraph_title_corner_columns_align() {
        // Subgraph title row has the form `┌─── 子图标题 ───┐`. The closing
        // `┐` must sit at the rightmost terminal column of the bottom-border
        // row's `┘`, i.e. the box is rectangular.
        let text = render_flowchart_fixture("cjk_subgraph.mmd");
        let lines: Vec<&str> = text.lines().collect();
        let mut checked = 0;
        for (idx, line) in lines.iter().enumerate() {
            let cols = terminal_columns_of(line);
            let chars: Vec<(char, usize)> = line.chars().zip(cols.iter().copied()).collect();
            let Some((_, tl_col)) = chars.iter().copied().find(|(c, _)| *c == '┌') else {
                continue;
            };
            let Some((_, tr_col)) = chars.iter().rev().copied().find(|(c, _)| *c == '┐') else {
                continue;
            };
            let bottom = (idx + 1..lines.len()).find_map(|j| {
                let row = lines[j];
                let bcols = terminal_columns_of(row);
                let bchars: Vec<(char, usize)> = row.chars().zip(bcols.iter().copied()).collect();
                let bl = bchars.iter().copied().find(|(c, _)| *c == '└')?.1;
                let br = bchars.iter().rev().copied().find(|(c, _)| *c == '┘')?.1;
                (bl == tl_col && br == tr_col).then_some(j)
            });
            assert!(
                bottom.is_some(),
                "flowchart/cjk_subgraph: subgraph at row {idx} (┌@{tl_col}, ┐@{tr_col}) must have a matching bottom row with └/┘ at the same columns\n{text}"
            );
            checked += 1;
        }
        assert!(
            checked > 0,
            "flowchart/cjk_subgraph: expected at least one subgraph border to validate\n{text}"
        );
    }
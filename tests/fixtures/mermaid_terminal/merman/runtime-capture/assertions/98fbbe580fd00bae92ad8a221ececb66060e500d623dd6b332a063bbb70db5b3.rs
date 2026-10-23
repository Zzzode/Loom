fn flowchart_issue_132_edge_labels_leave_visible_line_segments() {
    for direction in ["LR", "RL"] {
        for arrow in ["-->", "<-->"] {
            for label in [
                "very long description",
                "very long descriptions",
                "很长的连接说明文字",
            ] {
                for (options, line) in [
                    (AsciiRenderOptions::ascii(), "--"),
                    (AsciiRenderOptions::unicode(), "──"),
                ] {
                    let input = format!(
                        "flowchart {direction}\nfirst {arrow}|{label}| second\nsecond --> third[many words are too much sometime]"
                    );
                    let rendered =
                        render_flowchart(&input, &options).expect("diagram should render");
                    let row = rendered
                        .lines()
                        .find(|row| row.contains(label))
                        .expect("edge label");
                    let (before, after) = row.split_once(label).expect("complete edge label");
                    assert!(
                        before.ends_with(line) && after.starts_with(line),
                        "label must leave at least two line cells on each side ({direction}, {arrow}):\n{rendered}"
                    );
                    assert_rectangular_terminal_grid(&rendered);
                }
            }
        }
    }
}
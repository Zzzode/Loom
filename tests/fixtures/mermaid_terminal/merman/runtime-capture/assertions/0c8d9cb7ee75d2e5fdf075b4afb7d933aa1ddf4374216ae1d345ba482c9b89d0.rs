fn flowchart_fork_join_axis_is_perpendicular_to_the_graph_direction() {
    let left_right = render_flowchart(
        "flowchart LR\nA@{ shape: fork, label: \"hidden\" }",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();
    assert_eq!(
        left_right.lines().filter(|line| line.trim() == "#").count(),
        7,
        "LR fork should be a vertical synchronization bar:\n{left_right}"
    );

    let top_down = render_flowchart(
        "flowchart TD\nA@{ shape: join, label: \"hidden\" }",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();
    assert!(
        top_down.lines().any(|line| line.contains("=======")),
        "TD join should be a horizontal synchronization bar:\n{top_down}"
    );
}
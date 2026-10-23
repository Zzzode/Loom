fn flowchart_parser_top_down_branch_merge_preserves_dagre_rank_and_connectivity() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TD\n",
            "    A[Start] --> B{Condition?}\n",
            "    B -->|Yes| C[Execute]\n",
            "    B -->|No| D[End]\n",
            "    C --> D\n",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .unwrap();

    for expected in ["Start", "Condition?", "Yes", "No", "Execute", "End"] {
        assert!(
            rendered.contains(expected),
            "branch merge should keep {expected:?} visible:\n{rendered}"
        );
    }
    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert!(line_index("Start") < line_index("Condition?"), "{rendered}");
    assert!(
        line_index("Condition?") < line_index("Execute"),
        "{rendered}"
    );
    assert!(line_index("Execute") < line_index("End"), "{rendered}");
    assert!(
        rendered
            .chars()
            .filter(|ch| matches!(ch, '▼' | '▲' | '►' | '◄'))
            .count()
            == 4,
        "all four semantic edges should retain target markers:\n{rendered}"
    );
}
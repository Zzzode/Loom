fn flowchart_parser_top_down_skip_edges_retain_every_terminal_marker() {
    for input in [
        "flowchart TD\n  A --> B\n  B --> C\n  A --> C\n",
        "flowchart TD\n  A --> B\n  A --> C\n  B --> C\n",
        "flowchart TD\n  B --> C\n  A --> B\n  A --> C\n",
        "flowchart TD\n  B --> C\n  A --> C\n  A --> B\n",
        "flowchart TD\n  A --> C\n  A --> B\n  B --> C\n",
        "flowchart TD\n  A --> C\n  B --> C\n  A --> B\n",
    ] {
        let rendered = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();
        let arrow_count = rendered
            .chars()
            .filter(|ch| matches!(ch, '>' | '<' | 'v' | '^'))
            .count();

        assert_eq!(
            arrow_count, 3,
            "all three directed edges should retain one terminal marker for {input:?}:\n{rendered}"
        );
    }
}
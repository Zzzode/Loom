fn flowchart_parser_rl_chain_mirrors_unicode_connectors() {
    let rendered = render_flowchart("flowchart RL\nA --> B", &AsciiRenderOptions::unicode())
        .expect("RL flowchart direction should mirror Unicode connectors and arrowheads");

    assert_eq!(
        rendered,
        concat!(
            "┌───┐     ┌───┐\n",
            "│   │     │   │\n",
            "│ B │◄────┤ A │\n",
            "│   │     │   │\n",
            "└───┘     └───┘\n",
        )
    );
}
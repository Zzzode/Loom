fn flowchart_parser_nested_subgraph_direction_override_keeps_child_group_as_a_movable_block() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TD\n",
            "subgraph outer\n",
            "    direction LR\n",
            "    A\n",
            "    subgraph inner\n",
            "        direction TD\n",
            "        B --> C\n",
            "    end\n",
            "    A --> B\n",
            "end\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("nested subgraph direction override should render as a movable child block");

    assert_eq!(
        normalize_ascii_art(&rendered),
        normalize_ascii_art(concat!(
            "+-------------------+\n",
            "|       outer       |\n",
            "|                   |\n",
            "|                   |\n",
            "|         +-------+ |\n",
            "|         | inner | |\n",
            "|         |       | |\n",
            "|         |       | |\n",
            "| +---+   | +---+ | |\n",
            "| |   |   | |   | | |\n",
            "| | A |---+>| B | | |\n",
            "| |   |   | |   | | |\n",
            "| +---+   | +---+ | |\n",
            "|         |   |   | |\n",
            "|         |   |   | |\n",
            "|         |   |   | |\n",
            "|         |   |   | |\n",
            "|         |   v   | |\n",
            "|         | +---+ | |\n",
            "|         | |   | | |\n",
            "|         | | C | | |\n",
            "|         | |   | | |\n",
            "|         | +---+ | |\n",
            "|         |       | |\n",
            "|         +-------+ |\n",
            "|                   |\n",
            "+-------------------+\n",
        ))
    );
}
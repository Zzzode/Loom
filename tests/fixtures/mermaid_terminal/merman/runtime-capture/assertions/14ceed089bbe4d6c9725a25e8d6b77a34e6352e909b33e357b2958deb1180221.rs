fn class_parser_horizontal_long_label_keeps_connector_attached_to_ports() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction LR\n",
            "A \"1\" <|--|> \"*\" B : relationship label that forces a wide lane",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("wide horizontal class relationship should render");

    let (_, a_column) = line_and_column_containing(&rendered, "| A |");
    let (_, b_column) = line_and_column_containing(&rendered, "| B |");
    let connector = rendered
        .lines()
        .find(|line| line.contains("<|") && line.contains("|>"))
        .unwrap_or_else(|| panic!("missing two-sided horizontal connector:\n{rendered}"));
    let between_boxes = &connector[a_column + "| A |".len()..b_column];

    assert!(
        !between_boxes.contains(' '),
        "the connector must span the complete port-to-port gap:\n{rendered}"
    );
    for expected in ["1", "*", "relationship label that forces a wide lane"] {
        assert!(
            rendered.contains(expected),
            "horizontal relation must preserve {expected:?}:\n{rendered}"
        );
    }
}
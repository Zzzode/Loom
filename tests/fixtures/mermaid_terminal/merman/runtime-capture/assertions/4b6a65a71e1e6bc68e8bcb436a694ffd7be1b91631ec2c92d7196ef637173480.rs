fn class_parser_namespace_direction_controls_relationless_siblings() {
    let render = |direction| {
        render_class(
            &format!(
                "classDiagram\ndirection {direction}\nnamespace Domain {{\n  class A\n  class B\n}}"
            ),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("namespace direction {direction} should render: {error}"))
    };

    let tb = render("TB");
    let bt = render("BT");
    let lr = render("LR");
    let rl = render("RL");

    assert!(
        first_line_index_containing(&tb, "| A |") < first_line_index_containing(&tb, "| B |"),
        "TB should preserve namespace declaration order:\n{tb}"
    );
    assert!(
        first_line_index_containing(&bt, "| B |") < first_line_index_containing(&bt, "| A |"),
        "BT should reverse namespace declaration order:\n{bt}"
    );

    let (lr_a_line, lr_a_column) = line_and_column_containing(&lr, "| A |");
    let (lr_b_line, lr_b_column) = line_and_column_containing(&lr, "| B |");
    assert_eq!(
        lr_a_line, lr_b_line,
        "LR siblings should share a row:\n{lr}"
    );
    assert!(
        lr_a_column < lr_b_column,
        "LR should preserve namespace declaration order:\n{lr}"
    );

    let (rl_a_line, rl_a_column) = line_and_column_containing(&rl, "| A |");
    let (rl_b_line, rl_b_column) = line_and_column_containing(&rl, "| B |");
    assert_eq!(
        rl_a_line, rl_b_line,
        "RL siblings should share a row:\n{rl}"
    );
    assert!(
        rl_b_column < rl_a_column,
        "RL should reverse namespace declaration order:\n{rl}"
    );
}
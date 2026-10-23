fn class_parser_direction_controls_terminal_layout() {
    let render = |direction| {
        render_class(
            &format!("classDiagram\ndirection {direction}\nclass A\nclass B\nA --> B"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|err| panic!("class direction {direction} should render: {err}"))
    };

    let tb = render("TB");
    let bt = render("BT");
    let lr = render("LR");
    let rl = render("RL");

    assert!(
        first_line_index_containing(&tb, "| A |") < first_line_index_containing(&tb, "| B |"),
        "TB should place A above B:\n{tb}"
    );
    assert!(
        first_line_index_containing(&bt, "| B |") < first_line_index_containing(&bt, "| A |"),
        "BT should place B above A:\n{bt}"
    );

    let (lr_a_line, lr_a_column) = line_and_column_containing(&lr, "| A |");
    let (lr_b_line, lr_b_column) = line_and_column_containing(&lr, "| B |");
    assert_eq!(lr_a_line, lr_b_line, "LR should share one row:\n{lr}");
    assert!(
        lr_a_column < lr_b_column,
        "LR should place A left of B:\n{lr}"
    );

    let (rl_a_line, rl_a_column) = line_and_column_containing(&rl, "| A |");
    let (rl_b_line, rl_b_column) = line_and_column_containing(&rl, "| B |");
    assert_eq!(rl_a_line, rl_b_line, "RL should share one row:\n{rl}");
    assert!(
        rl_b_column < rl_a_column,
        "RL should place B left of A:\n{rl}"
    );

    assert_ne!(tb, bt);
    assert_ne!(tb, lr);
    assert_ne!(lr, rl);

    let mut lowercase_model =
        parse_class_model("classDiagram\ndirection LR\nclass A\nclass B\nA --> B");
    lowercase_model.direction = "lr".to_string();
    assert_eq!(
        render_class_model(&lowercase_model, &AsciiRenderOptions::ascii())
            .expect("lowercase direct-model class direction should render"),
        lr
    );
}
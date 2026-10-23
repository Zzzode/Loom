fn class_parser_self_relation_renders_single_box_with_loop() {
    let rendered = render_class(
        "classDiagram\nclass Node\nNode --> Node : next",
        &AsciiRenderOptions::ascii(),
    )
    .expect("self class relation should render");

    assert_eq!(
        rendered,
        concat!(
            "+------+\n",
            "| Node |---+\n",
            "+------+   |\n",
            "   next    |\n",
            "    v------+\n",
        )
    );
}
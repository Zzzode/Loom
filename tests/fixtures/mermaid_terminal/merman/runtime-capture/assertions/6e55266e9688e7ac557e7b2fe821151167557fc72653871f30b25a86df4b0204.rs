fn class_parser_parallel_self_relations_share_single_box_loop() {
    let rendered = render_class(
        "classDiagram\nclass Node\nNode --> Node : next\nNode ..> Node : loads",
        &AsciiRenderOptions::ascii(),
    )
    .expect("parallel self class relations should render");

    assert_eq!(
        rendered,
        concat!(
            "+------+\n",
            "| Node |---+\n",
            "+------+   |\n",
            "   next    |\n",
            "    v------+\n",
            "  loads    :\n",
            "    v......+\n",
        )
    );
}
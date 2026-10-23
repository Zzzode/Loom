fn er_parser_self_relationship_renders_single_box_with_loop() {
    let rendered = render_er(
        "erDiagram\nNODE ||--o{ NODE : \"leads to\"",
        &AsciiRenderOptions::ascii(),
    )
    .expect("self ER relationship should render");

    assert_eq!(
        rendered,
        concat!(
            "+------+\n",
            "| NODE |---||\n",
            "+------+   |\n",
            " leads to  |\n",
            "    o{-----+\n",
        )
    );
}
fn er_parser_crossing_relationship_layout_reorders_layer_to_render_each_edge() {
    let rendered = render_er(
        "erDiagram\nA ||--|| D : owns\nB ||--|| C : owns",
        &AsciiRenderOptions::ascii(),
    )
    .expect("crossing ER relationships should render by reordering the lower layer");

    assert_eq!(
        rendered,
        concat!(
            "+---+    +---+\n",
            "| A |    | B |\n",
            "+---+    +---+\n",
            " ||       ||\n",
            "owns     owns\n",
            "  |        |\n",
            " ||       ||\n",
            "+---+    +---+\n",
            "| D |    | C |\n",
            "+---+    +---+\n",
        )
    );
}
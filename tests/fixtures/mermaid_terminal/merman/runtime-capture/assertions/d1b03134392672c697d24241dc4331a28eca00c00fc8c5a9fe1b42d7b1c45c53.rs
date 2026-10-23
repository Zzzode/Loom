fn er_parser_parallel_self_relationships_share_single_box_loop() {
    let rendered = render_er(
        "erDiagram\nNODE ||--o{ NODE : \"leads to\"\nNODE o|--|| NODE : mirrors",
        &AsciiRenderOptions::ascii(),
    )
    .expect("parallel self ER relationships should render");

    assert_eq!(
        rendered,
        concat!(
            "+------+\n",
            "| NODE |----||\n",
            "+------+    |\n",
            " leads to   |\n",
            "    o{------+\n",
            "o| mirrors  |\n",
            "    ||------+\n",
        )
    );
}
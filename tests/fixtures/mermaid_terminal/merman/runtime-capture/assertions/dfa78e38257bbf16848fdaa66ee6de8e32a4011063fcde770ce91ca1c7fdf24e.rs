fn flowchart_parser_subroutine_and_cylinder_shapes_render_terminal_approximations() {
    let rendered = render_flowchart(
        "flowchart LR\nA[[Sub]] --> B[(DB)]",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+-------+     /------\\\n",
            "| |   | |     |------|\n",
            "| |Sub| |---->|  DB  |\n",
            "| |   | |     |      |\n",
            "+-------+     \\------/\n",
        )
    );
}
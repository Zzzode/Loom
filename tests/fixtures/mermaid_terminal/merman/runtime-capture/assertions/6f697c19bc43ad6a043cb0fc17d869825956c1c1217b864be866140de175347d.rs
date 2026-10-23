fn flowchart_style_color_truecolor_maps_class_statement_to_node_and_subgraph_foreground_without_plain_text_changes()
 {
    let input = concat!(
        "flowchart TB\n",
        "  subgraph sg [Group]\n",
        "    A[Alpha]\n",
        "  end\n",
        "  classDef warm color:#010203,stroke:#040506\n",
        "  class sg warm\n",
        "  class A warm\n",
    );
    let options = AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::TrueColor);

    let rendered = render_flowchart(input, &options).unwrap();
    let plain = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(strip_ansi(&rendered), plain);
    for expected_code in ["\u{1b}[38;2;1;2;3m", "\u{1b}[38;2;4;5;6m"] {
        assert!(
            rendered.contains(expected_code),
            "missing {expected_code:?} in {rendered:?}"
        );
    }
}
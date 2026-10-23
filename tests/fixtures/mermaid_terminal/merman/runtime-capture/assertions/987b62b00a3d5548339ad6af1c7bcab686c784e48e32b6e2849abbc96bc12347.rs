fn nested_subgraph_backgrounds_preserve_the_authored_child_fill() {
    let input = concat!(
        "flowchart TB\n",
        "  subgraph Outer [Outer]\n",
        "    subgraph Inner [Inner]\n",
        "      A[Alpha]\n",
        "    end\n",
        "  end\n",
        "  classDef outer fill:#ff0000\n",
        "  classDef inner fill:#0000ff\n",
        "  class Outer outer\n",
        "  class Inner inner\n",
    );
    let options = AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::TrueColor);

    let rendered = render_flowchart(input, &options).unwrap();
    let plain = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(strip_ansi(&rendered), plain);
    assert!(
        rendered.contains("\u{1b}[48;2;255;0;0m"),
        "missing outer group fill in {rendered:?}"
    );
    assert!(
        rendered.contains("\u{1b}[48;2;0;0;255m"),
        "the parent background must not overwrite the nested group fill in {rendered:?}"
    );
}
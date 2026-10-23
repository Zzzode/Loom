fn xychart_parser_line_plot_renders_ascii_stair_step_line() {
    let rendered = render_xychart(
        r#"xychart
x-axis [A, B, C]
y-axis 0 --> 10
line [1, 5, 9]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("xychart line plot should render");

    assert_eq!(
        rendered,
        concat!(
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\", bytes=1=\"C\"]\n",
            "values: * series=0 type=line title=none samples=[{index=0 x(bytes=1)=\"A\" value=1 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=5 pointLabel=none clipped=false}, {index=2 x(bytes=1)=\"C\" value=9 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "10 +       +-*\n",
            " 8 +       |\n",
            " 6 +   +-*-+\n",
            " 4 +   |\n",
            " 2 + *-+\n",
            " 0 +-+---+---+-\n",
            "     A   B   C\n",
        )
    );
}
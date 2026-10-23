fn xychart_parser_horizontal_line_uses_terminal_value_disclosure() {
    let rendered = render_xychart(
        r#"%%{init: {"xyChart": {"showDataLabel": true}}}%%
xychart horizontal
x-axis [A, B]
y-axis 0 --> 10
line [4, 8]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal xychart line plot should render with terminal value disclosure");

    assert_eq!(
        rendered,
        concat!(
            "values: * series=0 type=line title=none samples=[{index=0 x(bytes=1)=\"A\" value=4 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=8 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "A +   *-+\n",
            "B +     +-*\n",
            "  ++--------+\n",
            "   0       10\n",
        )
    );
}
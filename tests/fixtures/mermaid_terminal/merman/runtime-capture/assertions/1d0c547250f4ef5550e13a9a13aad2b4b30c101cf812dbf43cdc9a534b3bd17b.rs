fn xychart_parser_mixed_bar_and_line_overlay_in_series_order() {
    let rendered = render_xychart(
        r#"xychart
x-axis [A, B]
y-axis 0 --> 10
bar [2, 8]
line [8, 2]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("mixed xychart should render");

    assert_eq!(
        rendered,
        concat!(
            "# Bar 1  * Line 1\n",
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]\n",
            "values: # series=0 type=bar title=none samples=[{index=0 x(bytes=1)=\"A\" value=2 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=8 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "values: * series=1 type=line title=none samples=[{index=0 x(bytes=1)=\"A\" value=8 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=2 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "10 +\n",
            " 8 + *-+###\n",
            " 6 +   |###\n",
            " 4 +   |###\n",
            " 2 +###+-*#\n",
            " 0 +-+---+-\n",
            "     A   B\n",
        )
    );
}
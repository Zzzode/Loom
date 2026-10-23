fn xychart_parser_vertical_bar_renders_ascii_chart_with_titles_and_axes() {
    let rendered = render_xychart(
        r#"xychart
title "Sales"
x-axis "Month" [Jan, Feb, Mar]
y-axis "Revenue" 0 --> 10
bar [2, 5, 8]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("xychart should render");

    assert_eq!(
        rendered,
        concat!(
            "titleDisplay: chart(bytes=5)=\"Sales\"\n",
            "titleDisplay: yAxis(bytes=7)=\"Revenue\"\n",
            "values: # series=0 type=bar title=none samples=[{index=0 x(bytes=3)=\"Jan\" value=2 pointLabel=none clipped=false}, {index=1 x(bytes=3)=\"Feb\" value=5 pointLabel=none clipped=false}, {index=2 x(bytes=3)=\"Mar\" value=8 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "10 +\n",
            " 8 +        ###\n",
            " 6 +    ### ###\n",
            " 4 +    ### ###\n",
            " 2 +### ### ###\n",
            " 0 +-+---+---+-\n",
            "    Jan Feb Mar\n",
            "titleDisplay: xAxis(bytes=5)=\"Month\"\n",
        )
    );
}
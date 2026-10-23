fn xychart_parser_multiseries_data_labels_use_terminal_value_disclosure() {
    let rendered = render_xychart(
        r#"%%{init: {"xyChart": {"showDataLabel": true}}}%%
xychart
x-axis [A, B]
y-axis 0 --> 10
bar "Revenue" [2, 8]
line "Forecast" [8, 2]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("multi-series xychart should render with terminal value disclosure");

    assert_eq!(
        rendered,
        concat!(
            "# Revenue  * Forecast\n",
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]\n",
            "values: # series=0 type=bar title(bytes=7)=\"Revenue\" samples=[{index=0 x(bytes=1)=\"A\" value=2 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=8 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "values: * series=1 type=line title(bytes=8)=\"Forecast\" samples=[{index=0 x(bytes=1)=\"A\" value=8 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=2 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
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
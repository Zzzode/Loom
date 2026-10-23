fn xychart_parser_infers_numeric_x_labels_when_x_axis_is_omitted() {
    let rendered = render_xychart(
        r#"xychart
y-axis 0 --> 10
bar [4]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("xychart with inferred x axis should render");

    assert_eq!(
        rendered,
        concat!(
            "xDomain: linear authored=[1,1] resolved=[1,1]\n",
            "values: # series=0 type=bar title=none samples=[{index=0 x(bytes=1)=\"1\" value=4 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "10 +\n",
            " 8 +\n",
            " 6 +\n",
            " 4 +###\n",
            " 2 +###\n",
            " 0 +-+-\n",
            "     1\n",
        )
    );
}
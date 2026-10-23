fn xychart_parser_multiple_same_type_series_render_legend_labels_by_type_order() {
    let rendered = render_xychart(
        r#"xychart
x-axis [A, B]
y-axis 0 --> 10
bar [2, 8]
bar [5, 6]
line [8, 2]
line [4, 4]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("multi-series xychart should render");

    let legend = rendered
        .lines()
        .next()
        .expect("multi-series chart should render a legend line");

    assert_eq!(legend, "# Bar 1  # Bar 2  * Line 1  * Line 2");
}
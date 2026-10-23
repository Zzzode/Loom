fn xychart_parser_uses_series_titles_in_legend_when_available() {
    let rendered = render_xychart(
        r#"xychart
x-axis [A, B]
y-axis 0 --> 10
bar "Revenue" [2, 8]
line "Forecast" [8, 2]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("xychart should render with series titles");

    let legend = rendered
        .lines()
        .next()
        .expect("xychart with series titles should render a legend line");

    assert_eq!(legend, "# Revenue  * Forecast");
}
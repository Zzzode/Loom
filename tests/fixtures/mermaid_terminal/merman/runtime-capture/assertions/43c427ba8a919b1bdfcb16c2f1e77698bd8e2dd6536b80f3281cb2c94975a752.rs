fn xychart_parser_title_owners_prevent_chart_axis_spoofing() {
    let chart_owned = render_xychart(
        concat!(
            "xychart\n",
            "title \"y: Y\"\n",
            "x-axis [A]\n",
            "y-axis 0 --> 10\n",
            "line [5]\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("parser chart-title fixture should render");
    let y_axis_owned = render_xychart(
        concat!(
            "xychart\n",
            "x-axis [A]\n",
            "y-axis Y 0 --> 10\n",
            "line [5]\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("parser y-axis-title fixture should render");

    assert_ne!(
        chart_owned, y_axis_owned,
        "parser-produced chart and y-axis titles must retain distinct owners"
    );
    assert!(
        chart_owned.contains(r#"titleDisplay: chart(bytes=4)="y: Y""#),
        "parser chart title lost its owner frame:\n{chart_owned}"
    );
    assert!(
        y_axis_owned.contains(r#"titleDisplay: yAxis(bytes=1)="Y""#),
        "parser y-axis title lost its owner frame:\n{y_axis_owned}"
    );
}
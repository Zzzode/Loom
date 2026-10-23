fn xychart_direct_model_title_owners_prevent_chart_axis_spoofing() {
    let render = |chart_title: Option<&str>, y_title: &str, show_chart: bool, show_y: bool| {
        let mut model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![5.0],
                data: vec![("A".to_string(), Some(5.0))],
                point_labels: Vec::new(),
            }],
        );
        model.title = chart_title.map(str::to_string);
        model.y_axis = XyChartAxisRenderModel::Linear {
            title: y_title.to_string(),
            min: Some(0.0),
            max: Some(10.0),
        };
        model.display.show_title = show_chart;
        model.display.y_axis.show_title = show_y;
        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("owned title disclosure should render")
    };

    let chart_owned = render(Some("y: Y"), "", true, false);
    let y_axis_owned = render(None, "Y", false, true);

    assert_ne!(
        chart_owned, y_axis_owned,
        "a chart title must not impersonate a renderer-owned y-axis title row"
    );
    assert!(
        chart_owned
            .lines()
            .any(|line| line == r#"titleDisplay: chart(bytes=4)="y: Y""#),
        "chart title lost its owner or UTF-8 byte frame:\n{chart_owned}"
    );
    assert!(
        y_axis_owned
            .lines()
            .any(|line| line == r#"titleDisplay: yAxis(bytes=1)="Y""#),
        "y-axis title lost its owner or UTF-8 byte frame:\n{y_axis_owned}"
    );
    assert!(
        !chart_owned.lines().any(|line| line == "y: Y"),
        "authored chart text must never be emitted as an unframed owned row:\n{chart_owned}"
    );

    let authored_owned_row = render(Some(r#"titleDisplay: yAxis(bytes=1)="Y""#), "", true, false);
    assert!(
        authored_owned_row
            .contains(r#"titleDisplay: chart(bytes=32)="titleDisplay: yAxis(bytes=1)=\"Y\"""#),
        "renderer-like title text must remain a chart-owned payload without prefix filtering:\n\
         {authored_owned_row}"
    );
}
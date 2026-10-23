fn xychart_nonempty_domain_discloses_empty_series_title_and_type() {
    let render = |plot_type, title: &str| {
        let model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type,
                title: Some(title.to_string()),
                values: Vec::new(),
                data: Vec::new(),
                point_labels: Vec::new(),
            }],
        );

        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("empty series metadata should render")
    };

    let line = render(XyChartPlotType::Line, "Forecast");
    let bar = render(XyChartPlotType::Bar, "Forecast");
    let actual = render(XyChartPlotType::Line, "Actual");

    assert!(
        line.contains(
            r#"values: * series=0 type=line title(bytes=8)="Forecast" samples=[] orphanPointLabels=[]"#
        ),
        "{line}"
    );
    assert!(
        bar.contains(
            r#"values: # series=0 type=bar title(bytes=8)="Forecast" samples=[] orphanPointLabels=[]"#
        ),
        "{bar}"
    );
    assert_ne!(line, bar);
    assert_ne!(line, actual);
}
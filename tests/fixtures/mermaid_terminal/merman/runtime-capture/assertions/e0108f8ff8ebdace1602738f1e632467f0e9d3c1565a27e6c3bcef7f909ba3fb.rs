fn xychart_exact_disclosure_quotes_control_and_field_delimiters() {
    let model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A\nB".to_string()],
        },
        0.0,
        2.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: Some("series=\"one\"\\two".to_string()),
            values: vec![2.0],
            data: vec![("A\nB".to_string(), Some(2.0))],
            point_labels: vec!["point=\"peak\"\\tail".to_string()],
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("quoted XYChart disclosure should render");

    assert!(
        rendered.contains(r#"title(bytes=16)="series=\"one\"\\two""#),
        "series title was not injectively quoted:\n{rendered}"
    );
    assert!(
        rendered.contains(r#"x(bytes=3)="A\nB""#),
        "structural newline was not escaped inside the framed x field:\n{rendered}"
    );
    assert!(
        rendered.contains(r#"pointLabel(bytes=17)="point=\"peak\"\\tail""#),
        "point label was not injectively quoted:\n{rendered}"
    );
}
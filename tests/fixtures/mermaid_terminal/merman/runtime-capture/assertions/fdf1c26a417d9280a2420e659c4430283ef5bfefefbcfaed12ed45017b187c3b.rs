fn xychart_degenerate_range_keeps_data_visible() {
    let model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string()],
        },
        5.0,
        5.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Bar,
            title: None,
            values: vec![5.0],
            data: vec![("A".to_string(), Some(5.0))],
            point_labels: Vec::new(),
        }],
    );

    let options = AsciiRenderOptions::ascii().with_xychart_vertical_plot_height(4);
    let rendered = render_typed_xychart(&model, &options)
        .expect("degenerate XYChart range should render deterministically");

    assert_eq!(
        rendered.lines().filter(|line| line.contains('#')).count(),
        2,
        "a degenerate linear scale should place its bar at the domain midpoint:\n{rendered}"
    );
}
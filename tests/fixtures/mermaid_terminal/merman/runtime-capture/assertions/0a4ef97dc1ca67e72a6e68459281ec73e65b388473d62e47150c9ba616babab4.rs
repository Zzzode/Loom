fn xychart_horizontal_line_series_draw_connected_paths() {
    let model = typed_xychart_model(
        "horizontal",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string(), "B".to_string(), "C".to_string()],
        },
        0.0,
        10.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: None,
            values: vec![2.0, 8.0, 4.0],
            data: vec![
                ("A".to_string(), Some(2.0)),
                ("B".to_string(), Some(8.0)),
                ("C".to_string(), Some(4.0)),
            ],
            point_labels: Vec::new(),
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("horizontal line series should render");
    let painted_cells = rendered
        .chars()
        .filter(|ch| matches!(ch, '*' | '-' | '|' | '+'))
        .count();

    assert!(
        painted_cells > 3,
        "three samples should be joined by path cells, not emitted as isolated points:\n{rendered}"
    );
    assert!(
        rendered.contains('-') && rendered.contains('+'),
        "horizontal line topology should expose segments and bends:\n{rendered}"
    );
}
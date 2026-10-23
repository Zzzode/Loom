fn xychart_reversed_value_range_preserves_authored_axis_direction() {
    let mut model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string(), "B".to_string()],
        },
        10.0,
        0.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: None,
            values: vec![10.0, 0.0],
            data: vec![("A".to_string(), Some(10.0)), ("B".to_string(), Some(0.0))],
            point_labels: Vec::new(),
        }],
    );
    model.display.x_axis = XyChartAxisDisplayPolicy::default();
    model.display.y_axis = XyChartAxisDisplayPolicy::default();

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("reversed XYChart range should render");
    let lines = rendered.lines().collect::<Vec<_>>();
    let top_endpoint = lines
        .iter()
        .position(|line| line.trim_start().starts_with("0 +"));
    let baseline_endpoint = lines
        .iter()
        .position(|line| line.trim_start().starts_with("10 +"));

    assert!(
        matches!((top_endpoint, baseline_endpoint), (Some(top), Some(bottom)) if top < bottom),
        "the authored `0` endpoint should remain at the top of a reversed axis:\n{rendered}"
    );
}
fn xychart_horizontal_linear_axis_labels_use_authored_sample_coordinates() {
    let mut model = typed_xychart_model(
        "horizontal",
        XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: Some(0.0),
            max: Some(10.0),
        },
        0.0,
        10.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: None,
            values: vec![2.0, 5.0, 8.0],
            data: vec![
                ("0".to_string(), Some(2.0)),
                ("4".to_string(), Some(5.0)),
                ("10".to_string(), Some(8.0)),
            ],
            point_labels: Vec::new(),
        }],
    );
    model.display.x_axis = XyChartAxisDisplayPolicy::default();

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("horizontal linear samples should retain their authored x labels");
    let labels = rendered
        .lines()
        .filter_map(|line| line.split_whitespace().next())
        .collect::<Vec<_>>();

    assert!(
        labels.contains(&"4"),
        "the x=4 sample was mislabeled:\n{rendered}"
    );
    assert!(
        !labels.contains(&"5"),
        "a generated midpoint tick must not replace the authored x=4 coordinate:\n{rendered}"
    );
    assert!(
        rendered.contains(r#"x(bytes=1)="4" value=5"#),
        "authored linear coordinates need exact disclosure:\n{rendered}"
    );
}
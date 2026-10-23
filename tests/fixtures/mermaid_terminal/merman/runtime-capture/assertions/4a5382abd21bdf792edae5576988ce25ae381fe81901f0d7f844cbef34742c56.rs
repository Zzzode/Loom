fn xychart_typed_data_is_the_coordinate_and_value_source_of_truth() {
    let mut model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: Some(0.0),
            max: Some(10.0),
        },
        -3.0,
        1.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: Some("precision".to_string()),
            values: vec![999.0, 998.0, 997.0],
            data: vec![
                ("0".to_string(), Some(0.001)),
                ("1".to_string(), Some(0.75)),
                ("10".to_string(), Some(-2.5)),
            ],
            point_labels: vec!["tiny".to_string(), "ratio".to_string(), "loss".to_string()],
        }],
    );
    model.display.show_data_label = true;

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("typed XYChart data should render");

    for expected in [
        "x(bytes=1)=\"0\" value=0.001 pointLabel(bytes=4)=\"tiny\"",
        "x(bytes=1)=\"1\" value=0.75 pointLabel(bytes=5)=\"ratio\"",
        "x(bytes=2)=\"10\" value=-2.5 pointLabel(bytes=4)=\"loss\"",
    ] {
        assert!(
            rendered.contains(expected),
            "typed sample {expected:?} should survive exact disclosure:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("999") && !rendered.contains("998") && !rendered.contains("997"),
        "legacy values must not override typed data samples:\n{rendered}"
    );
}
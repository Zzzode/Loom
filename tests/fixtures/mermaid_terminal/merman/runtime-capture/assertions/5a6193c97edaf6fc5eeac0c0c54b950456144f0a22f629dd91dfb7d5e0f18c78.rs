fn xychart_rejects_direct_model_band_y_axis() {
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
    model.y_axis = XyChartAxisRenderModel::Band {
        title: "invalid".to_string(),
        categories: vec!["low".to_string(), "high".to_string()],
    };

    let error = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect_err("Mermaid-invalid Band y-axes must not be silently linearized");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "xychart",
            feature: "band y-axis"
        }
    ));
}
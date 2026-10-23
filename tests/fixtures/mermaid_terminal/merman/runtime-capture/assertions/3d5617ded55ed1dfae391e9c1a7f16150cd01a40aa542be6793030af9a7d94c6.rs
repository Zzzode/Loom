fn xychart_rejects_unknown_direct_model_orientation() {
    let model = typed_xychart_model(
        "diagonal",
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

    let error = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect_err("unknown direct-model orientations must not become vertical charts");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "xychart",
            feature: "chart orientation"
        }
    ));
}
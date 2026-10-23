fn xychart_accessibility_metadata_is_intentionally_omitted() {
    let model = typed_xychart_model(
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
    let mut with_accessibility_metadata = model.clone();
    with_accessibility_metadata.acc_title = Some("Screen reader title".to_string());
    with_accessibility_metadata.acc_descr = Some("Browser-only chart description".to_string());

    let baseline = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("baseline XYChart should render");
    let with_accessibility_metadata =
        render_typed_xychart(&with_accessibility_metadata, &AsciiRenderOptions::ascii())
            .expect("accessibility metadata should not block terminal rendering");

    assert_eq!(
        baseline, with_accessibility_metadata,
        "browser accessibility metadata is an intentional terminal omission"
    );
}
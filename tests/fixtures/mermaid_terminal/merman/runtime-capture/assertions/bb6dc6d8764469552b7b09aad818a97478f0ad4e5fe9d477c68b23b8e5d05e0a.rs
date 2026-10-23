fn xychart_grouped_horizontal_extent_is_budgeted_before_allocation() {
    let model = typed_xychart_model(
        "horizontal",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string(), "B".to_string()],
        },
        0.0,
        10.0,
        vec![
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![2.0, 8.0],
                data: Vec::new(),
                point_labels: Vec::new(),
            },
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![7.0, 3.0],
                data: Vec::new(),
                point_labels: Vec::new(),
            },
        ],
    );
    let options = AsciiRenderOptions::ascii().with_xychart_horizontal_plot_width(10);
    let resources = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxGridCells, 39)
        .expect("valid grid limit");

    let error = render_typed_xychart_with_resources(&model, &options, resources)
        .expect_err("the four-by-ten grouped plot must exceed 39 grid cells");
    let AsciiError::ResourceLimitExceeded(details) = error else {
        panic!("expected resource-limit error, got {error:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxGridCells);
    assert_eq!(details.actual, 40);
    assert_eq!(details.max, 39);
}
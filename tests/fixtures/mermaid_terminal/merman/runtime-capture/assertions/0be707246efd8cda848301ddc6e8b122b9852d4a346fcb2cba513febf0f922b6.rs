fn xychart_vertical_grouped_bars_use_distinct_category_lanes() {
    let model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string()],
        },
        0.0,
        10.0,
        vec![
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: Some("full".to_string()),
                values: vec![10.0],
                data: vec![("A".to_string(), Some(10.0))],
                point_labels: Vec::new(),
            },
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: Some("half".to_string()),
                values: vec![5.0],
                data: vec![("A".to_string(), Some(5.0))],
                point_labels: Vec::new(),
            },
        ],
    );
    let options = AsciiRenderOptions::ascii()
        .with_xychart_vertical_plot_height(4)
        .with_xychart_category_band_width(4);

    let rendered =
        render_typed_xychart(&model, &options).expect("grouped vertical bars should render");
    let plot_rows = rendered
        .lines()
        .filter(|line| !line.is_empty() && line.chars().all(|ch| ch == '#'))
        .collect::<Vec<_>>();

    assert_eq!(
        plot_rows,
        vec!["##", "##", "####", "####"],
        "bar series should occupy independent lanes inside the category band:\n{rendered}"
    );
}
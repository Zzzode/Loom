fn xychart_empty_projection_html_escapes_authored_fields() {
    let model = XyChartDiagramRenderModel {
        orientation: "vertical".to_string(),
        title: Some("<empty & safe>".to_string()),
        acc_title: None,
        acc_descr: None,
        x_axis: XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: Vec::new(),
        },
        y_axis: XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: None,
            max: None,
        },
        plots: Vec::new(),
        display: XyChartDisplayPolicy::default(),
    };
    let rendered = render_typed_xychart(
        &model,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html),
    )
    .expect("empty XYChart HTML output should render");

    assert!(rendered.contains("title(bytes=14)=&quot;&lt;empty &amp; safe&gt;&quot;"));
    assert!(!rendered.contains("<empty"));
}
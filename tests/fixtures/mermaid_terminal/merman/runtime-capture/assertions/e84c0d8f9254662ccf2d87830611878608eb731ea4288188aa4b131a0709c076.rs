fn xychart_empty_plot_preserves_typed_series_metadata() {
    let model = XyChartDiagramRenderModel {
        orientation: "vertical".to_string(),
        title: Some("No samples".to_string()),
        acc_title: None,
        acc_descr: None,
        x_axis: XyChartAxisRenderModel::Band {
            title: "Category".to_string(),
            categories: Vec::new(),
        },
        y_axis: XyChartAxisRenderModel::Linear {
            title: "Score".to_string(),
            min: Some(0.0),
            max: Some(10.0),
        },
        plots: vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: Some("Forecast".to_string()),
            values: Vec::new(),
            data: Vec::new(),
            point_labels: vec!["orphan".to_string()],
        }],
        display: XyChartDisplayPolicy::default(),
    };

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("zero-slot typed XYChart should retain its authored fields");

    assert_eq!(
        rendered,
        concat!(
            "xychart: empty\n",
            "orientation: vertical\n",
            "title(bytes=10)=\"No samples\"\n",
            "xAxis: band title(bytes=8)=\"Category\" categories=[]\n",
            "yAxis: linear title(bytes=5)=\"Score\" min=0 max=10\n",
            "display: showTitle=true showDataLabel=false showDataLabelOutsideBar=false xAxis={showLabel=true showTitle=true showTick=true showAxisLine=true} yAxis={showLabel=true showTitle=true showTick=true showAxisLine=true}\n",
            "plots: count=1\n",
            "plot: index=0 type=line title(bytes=8)=\"Forecast\" values=[] data=[] pointLabels=[bytes=6 \"orphan\"]",
        )
    );
}
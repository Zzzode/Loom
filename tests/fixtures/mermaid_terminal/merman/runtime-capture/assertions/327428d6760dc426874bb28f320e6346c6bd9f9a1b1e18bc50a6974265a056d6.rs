fn xychart_orphan_point_labels_and_clipped_values_are_disclosed() {
    let model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string()],
        },
        0.0,
        1.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: None,
            values: vec![2.0],
            data: vec![("A".to_string(), Some(2.0))],
            point_labels: vec!["peak".to_string(), "detached".to_string()],
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("clipped and orphan XYChart semantics should render through disclosure");

    assert!(
        rendered.contains("x(bytes=1)=\"A\" value=2 pointLabel(bytes=4)=\"peak\" clipped=true"),
        "clipped value and anchored point label should remain exact:\n{rendered}"
    );
    assert!(
        rendered.contains("orphanPointLabels=[bytes=8=\"detached\"]"),
        "orphan point labels must not disappear silently:\n{rendered}"
    );
}
fn xychart_trailing_band_projection_discloses_trimmed_category() {
    for orientation in ["vertical", "horizontal"] {
        let mut model = typed_xychart_model(
            orientation,
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A  ".to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![5.0],
                data: vec![("A  ".to_string(), Some(5.0))],
                point_labels: Vec::new(),
            }],
        );
        model.display.x_axis.show_label = true;

        let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("trailing Band category should render through framed disclosure");
        assert!(
            rendered.contains(r#"xDomain: band categories=[bytes=3="A  "]"#),
            "a category erased by the final row trim must retain its source identity:\n{rendered}"
        );
    }
}
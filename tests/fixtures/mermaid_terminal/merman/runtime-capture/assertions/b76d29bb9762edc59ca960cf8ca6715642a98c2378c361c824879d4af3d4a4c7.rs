fn xychart_centered_band_projection_discloses_colliding_categories() {
    for (orientation, second) in [("vertical", " A "), ("horizontal", "  A")] {
        let mut model = typed_xychart_model(
            orientation,
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string(), second.to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![4.0, 8.0],
                data: vec![
                    ("A".to_string(), Some(4.0)),
                    (second.to_string(), Some(8.0)),
                ],
                point_labels: Vec::new(),
            }],
        );
        model.display.x_axis.show_label = true;
        let options = AsciiRenderOptions::ascii().with_xychart_category_band_width(3);

        let rendered = render_typed_xychart(&model, &options)
            .expect("projected Band labels should render through exact disclosure");
        let expected = format!(
            r#"xDomain: band categories=[bytes=1="A", bytes={}="{second}"]"#,
            second.len()
        );

        assert!(
            rendered.contains(&expected),
            "{orientation} categories whose final projection loses identity must remain explicit:\n{rendered}"
        );
        assert!(
            !rendered.lines().any(|line| line.starts_with("values: ")),
            "domain-only disclosure should not add a redundant values row:\n{rendered}"
        );
    }
}
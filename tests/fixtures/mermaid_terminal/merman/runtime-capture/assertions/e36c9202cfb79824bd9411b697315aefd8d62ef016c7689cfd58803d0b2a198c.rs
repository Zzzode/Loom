fn xychart_empty_band_domain_discloses_authored_sample_coordinates() {
    for orientation in ["vertical", "horizontal"] {
        let render = |x: &str| {
            let model = typed_xychart_model(
                orientation,
                XyChartAxisRenderModel::Band {
                    title: String::new(),
                    categories: Vec::new(),
                },
                0.0,
                10.0,
                vec![XyChartPlotRenderModel {
                    plot_type: XyChartPlotType::Line,
                    title: None,
                    values: vec![5.0],
                    data: vec![(x.to_string(), Some(5.0))],
                    point_labels: Vec::new(),
                }],
            );
            render_typed_xychart(&model, &AsciiRenderOptions::ascii())
                .unwrap_or_else(|error| panic!("{orientation} empty Band should render: {error}"))
        };

        let alpha = render("alpha");
        let beta = render("beta");
        assert!(
            alpha.contains("x(bytes=5)=\"alpha\" value=5"),
            "{orientation} output must disclose the authored x coordinate:\n{alpha}"
        );
        assert!(
            beta.contains("x(bytes=4)=\"beta\" value=5"),
            "{orientation} output must disclose the authored x coordinate:\n{beta}"
        );
        assert_ne!(
            alpha, beta,
            "distinct authored x coordinates must not collapse to the same output"
        );
    }
}
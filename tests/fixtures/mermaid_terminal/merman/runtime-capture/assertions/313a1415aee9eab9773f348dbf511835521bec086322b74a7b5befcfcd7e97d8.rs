fn xychart_implicit_and_explicit_band_domains_remain_injective() {
    let build = |categories, show_x_labels| {
        let mut model = typed_xychart_model(
            "horizontal",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories,
            },
            0.0,
            1.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![1.0],
                data: Vec::new(),
                point_labels: Vec::new(),
            }],
        );
        if show_x_labels {
            model.display.x_axis = XyChartAxisDisplayPolicy::default();
        }
        model
    };

    for show_x_labels in [false, true] {
        let implicit = render_typed_xychart(
            &build(Vec::new(), show_x_labels),
            &AsciiRenderOptions::ascii(),
        )
        .expect("the implicit Band domain should render");
        let explicit = render_typed_xychart(
            &build(vec!["1".to_string()], show_x_labels),
            &AsciiRenderOptions::ascii(),
        )
        .expect("the explicit Band domain should render");

        assert_ne!(implicit, explicit, "Band domain ownership must be exact");
        assert!(
            implicit.contains("xDomain: band categories=[]"),
            "{implicit}"
        );
    }
}
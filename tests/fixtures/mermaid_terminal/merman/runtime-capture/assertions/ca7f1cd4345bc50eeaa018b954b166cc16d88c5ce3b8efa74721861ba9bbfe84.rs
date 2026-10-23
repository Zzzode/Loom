fn flowchart_typed_model_rejects_icon_and_image_metadata() {
    let mut icon = single_node_flowchart_model("rect", "Icon");
    icon.nodes[0].icon = Some("fa:circle".to_string());
    let mut image = single_node_flowchart_model("rect", "Image");
    image.nodes[0].img = Some("data:image/png;base64,AA==".to_string());

    for model in [icon, image] {
        let error = render_model(
            &RenderSemanticModel::Flowchart(model),
            &AsciiRenderOptions::ascii(),
        )
        .expect_err("icon and image metadata must not silently render as a plain node");
        assert_eq!(
            error,
            AsciiError::UnsupportedFeature {
                diagram_type: "flowchart",
                feature: "flowchart icon and image node metadata",
            }
        );
    }
}
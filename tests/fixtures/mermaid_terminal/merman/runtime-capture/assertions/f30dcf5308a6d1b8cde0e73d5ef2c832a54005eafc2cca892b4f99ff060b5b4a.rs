fn flowchart_typed_model_internal_shape_aliases_render() {
    let cases = [
        ("forkJoin", "F", false),
        ("stateStart", "S", false),
        ("stateEnd", "E", false),
        ("rect_left_inv_arrow", "Odd", true),
    ];

    for (shape, label, should_show_label) in cases {
        let model = RenderSemanticModel::Flowchart(single_node_flowchart_model(shape, label));
        let rendered = render_model(&model, &AsciiRenderOptions::ascii()).unwrap();

        if should_show_label {
            assert!(
                rendered.contains(label),
                "{shape} internal shape should keep its label:\n{rendered}"
            );
        } else {
            assert!(
                !rendered.contains(label),
                "{shape} internal shape should not keep its label:\n{rendered}"
            );
        }
    }
}
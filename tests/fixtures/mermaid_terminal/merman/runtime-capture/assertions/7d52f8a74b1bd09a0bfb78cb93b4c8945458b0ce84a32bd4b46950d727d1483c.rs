fn tree_view_rejects_unknown_direct_model_node_types() {
    let mut model = TreeViewDiagramRenderModel::default();
    model.root.node_type = "mystery".to_string();

    let error = render_model(
        &RenderSemanticModel::TreeView(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("unknown TreeView node types must not be projected as authored syntax");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "treeView",
            feature: "unknown node types",
        }
    );
}
fn tree_view_rejects_duplicate_public_node_ids() {
    let model = TreeViewDiagramRenderModel {
        root: tree_node(
            0,
            -1,
            "workspace",
            vec![
                tree_node(1, 0, "first", Vec::new()),
                tree_node(1, 0, "second", Vec::new()),
            ],
        ),
        ..Default::default()
    };

    let error = render_model(
        &RenderSemanticModel::TreeView(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate TreeView identities must be rejected before projection");

    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "treeView",
            feature: "duplicate node ids",
        }
    );
}
fn tree_view_structured_text_framing_distinguishes_icon_from_class() {
    let mut embedded_class = TreeViewDiagramRenderModel::default();
    embedded_class.root.icon = Some("folder, class=highlight".to_string());

    let mut explicit_class = TreeViewDiagramRenderModel::default();
    explicit_class.root.icon = Some("folder".to_string());
    explicit_class.root.css_class = Some("highlight".to_string());

    assert_ne!(
        render(RenderSemanticModel::TreeView(embedded_class)),
        render(RenderSemanticModel::TreeView(explicit_class)),
        "authored icon text must not be able to forge a CSS class field",
    );
}
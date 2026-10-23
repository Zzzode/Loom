fn mindmap_structured_text_discloses_identity_shape_icon_and_section() {
    let mut root = mindmap_node("root", "Root", 0);
    root.shape = "circle".to_string();
    root.icon = Some("home".to_string());
    root.section = Some(2);

    let rendered = render(RenderSemanticModel::Mindmap(MindmapDiagramRenderModel {
        nodes: vec![root],
        edges: Vec::new(),
    }));

    assert_eq!(
        rendered,
        concat!(
            "node(bytes=4)=\"Root\" id(bytes=4)=\"root\" shape(bytes=6)=\"circle\"\n",
            "icon(bytes=4)=\"home\" section=2",
        )
    );
}
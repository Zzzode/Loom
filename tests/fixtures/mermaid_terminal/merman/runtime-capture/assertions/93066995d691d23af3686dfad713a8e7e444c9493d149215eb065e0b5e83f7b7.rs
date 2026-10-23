fn mindmap_honors_ascii_and_unicode_structural_charsets() {
    let model = MindmapDiagramRenderModel {
        nodes: vec![
            mindmap_node("root", "Root", 0),
            mindmap_node("child", "Child", 1),
        ],
        edges: vec![mindmap_edge("edge", "root", "child")],
    };

    let ascii = render_with_options(
        RenderSemanticModel::Mindmap(model.clone()),
        &AsciiRenderOptions::ascii(),
    );
    let unicode = render_with_options(
        RenderSemanticModel::Mindmap(model),
        &AsciiRenderOptions::unicode(),
    );

    assert!(ascii.contains("\\-- "), "missing ASCII connector:\n{ascii}");
    assert!(
        unicode.contains("└── "),
        "missing Unicode connector:\n{unicode}"
    );
    assert_ne!(ascii, unicode);
}
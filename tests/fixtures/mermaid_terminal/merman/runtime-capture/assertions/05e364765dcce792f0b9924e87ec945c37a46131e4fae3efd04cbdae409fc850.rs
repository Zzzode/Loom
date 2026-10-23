fn mindmap_direct_model_rejects_duplicate_ids_and_missing_edges() {
    let mut duplicate = MindmapDiagramRenderModel {
        nodes: vec![mindmap_node("same", "A", 0), mindmap_node("same", "B", 0)],
        edges: Vec::new(),
    };
    let error = render_model(
        &RenderSemanticModel::Mindmap(duplicate.clone()),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate node ids must be rejected");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "mindmap",
            feature: "duplicate node ids",
        }
    ));

    duplicate.nodes = vec![mindmap_node("root", "Root", 0)];
    duplicate.edges = vec![mindmap_edge("edge", "root", "missing")];
    let error = render_model(
        &RenderSemanticModel::Mindmap(duplicate),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("missing edge endpoints must be rejected");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "mindmap",
            feature: "edge with missing end node",
        }
    ));

    let mut first = mindmap_node("0", "First", 0);
    first.node_id = "authored".to_string();
    let mut second = mindmap_node("1", "Second", 0);
    second.node_id = "authored".to_string();
    let duplicate_authored = MindmapDiagramRenderModel {
        nodes: vec![first, second],
        edges: Vec::new(),
    };
    let error = render_model(
        &RenderSemanticModel::Mindmap(duplicate_authored),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate authored node ids must be rejected");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "mindmap",
            feature: "duplicate authored node ids",
        }
    ));

    let mut missing_authored = mindmap_node("0", "Missing", 0);
    missing_authored.node_id.clear();
    let error = render_model(
        &RenderSemanticModel::Mindmap(MindmapDiagramRenderModel {
            nodes: vec![missing_authored],
            edges: Vec::new(),
        }),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("missing authored node ids must be rejected");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "mindmap",
            feature: "missing authored node ids",
        }
    ));
}
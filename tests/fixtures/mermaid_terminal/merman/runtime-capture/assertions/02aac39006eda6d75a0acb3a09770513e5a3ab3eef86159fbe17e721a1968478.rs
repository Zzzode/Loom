fn flowchart_direct_model_duplicate_subgraph_ids_use_the_parser_semantics() {
    let mut model = single_node_flowchart_model("squareRect", "A");
    let mut second_node = model.nodes[0].clone();
    second_node.id = "B".to_string();
    second_node.label = Some("B".to_string());
    model.nodes.push(second_node);
    model.subgraphs = vec![
        merman_core::diagrams::flowchart::FlowSubgraph {
            metadata: None,
            id: "X".to_string(),
            title: "First".to_string(),
            dir: None,
            has_explicit_dir: false,
            label_type: None,
            classes: Vec::new(),
            styles: Vec::new(),
            nodes: vec!["A".to_string()],
        },
        merman_core::diagrams::flowchart::FlowSubgraph {
            metadata: None,
            id: "X".to_string(),
            title: "Second".to_string(),
            dir: None,
            has_explicit_dir: false,
            label_type: None,
            classes: Vec::new(),
            styles: Vec::new(),
            nodes: vec!["B".to_string()],
        },
    ];

    let rendered = render_model(
        &RenderSemanticModel::Flowchart(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect("the direct render model should use the same merge semantics");

    assert!(rendered.contains("First"), "{rendered}");
    assert!(!rendered.contains("Second"), "{rendered}");
    for member in [" A ", " B "] {
        assert_eq!(rendered.matches(member).count(), 1, "{rendered}");
    }
}
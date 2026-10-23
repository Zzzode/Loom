fn duplicate_flowchart_parent_contains_its_nested_child_layout() {
    let mut model = single_node_flowchart_model("squareRect", "A");
    model.direction = Some("TB".to_string());
    let mut second_node = model.nodes[0].clone();
    second_node.id = "B".to_string();
    second_node.label = Some("B".to_string());
    model.nodes.push(second_node);
    model.subgraphs = vec![
        merman_core::diagrams::flowchart::FlowSubgraph {
            metadata: None,
            id: "X".to_string(),
            title: "First X".to_string(),
            dir: None,
            has_explicit_dir: false,
            label_type: None,
            classes: Vec::new(),
            styles: Vec::new(),
            nodes: vec!["A".to_string()],
        },
        merman_core::diagrams::flowchart::FlowSubgraph {
            metadata: None,
            id: "Y".to_string(),
            title: "Inner Y".to_string(),
            dir: None,
            has_explicit_dir: false,
            label_type: None,
            classes: Vec::new(),
            styles: Vec::new(),
            nodes: vec!["B".to_string()],
        },
        merman_core::diagrams::flowchart::FlowSubgraph {
            metadata: None,
            id: "X".to_string(),
            title: "Later X".to_string(),
            dir: None,
            has_explicit_dir: false,
            label_type: None,
            classes: Vec::new(),
            styles: Vec::new(),
            nodes: vec!["Y".to_string()],
        },
    ];

    let rendered = render_model(
        &RenderSemanticModel::Flowchart(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect("duplicate parent declarations should retain nested child containment");
    let outer = ascii_group_box(&rendered, "First X");
    let inner = ascii_group_box(&rendered, "Inner Y");

    assert!(!rendered.contains("Later X"), "{rendered}");
    assert!(
        outer.0 < inner.0,
        "outer left must precede inner: {rendered}"
    );
    assert!(
        outer.1 < inner.1,
        "outer top must precede inner: {rendered}"
    );
    assert!(
        outer.2 > inner.2,
        "outer right must follow inner: {rendered}"
    );
    assert!(
        outer.3 > inner.3,
        "outer bottom must follow inner: {rendered}"
    );
}
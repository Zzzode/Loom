fn authored_node_colliding_with_a_subgraph_uses_group_first_projection() {
    let source = concat!(
        "flowchart-elk TD\n",
        "G\n",
        "subgraph G\n",
        "  A\n",
        "end\n",
        "G --> A\n",
    );
    let parsed = Engine::new()
        .parse_diagram_for_render_model_sync(source, ParseOptions::strict())
        .expect("authored node/subgraph collision should parse")
        .expect("flowchart should be detected");
    let RenderSemanticModel::Flowchart(model) = parsed.model() else {
        panic!("expected a flowchart model");
    };
    let authored = model
        .nodes
        .iter()
        .find(|node| node.id == "G")
        .expect("authored G node");
    assert_eq!(authored.provenance, FlowNodeProvenance::Authored);

    let rendered = render_parsed(&parsed, &AsciiRenderOptions::ascii())
        .expect("authored node/subgraph collision should render through the group");
    assert_eq!(
        rendered.matches('G').count(),
        1,
        "Mermaid's group-first projection must not create a second leaf node:\n{rendered}"
    );
}
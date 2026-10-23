fn standalone_shape_data_does_not_override_same_id_group_projection() {
    let source = concat!(
        "flowchart-elk TD\n",
        "subgraph G\n",
        "  A\n",
        "end\n",
        "G --> A\n",
        "G@{ shape: rect, label: \"Authored G\" }\n",
    );
    let parsed = Engine::new()
        .parse_diagram_for_render_model_sync(source, ParseOptions::strict())
        .expect("standalone shapeData should parse")
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
        .expect("standalone shapeData should retain the group-first projection");
    assert!(
        !rendered.contains("Authored G"),
        "same-id vertex labels do not replace the Mermaid subgraph title:\n{rendered}"
    );
    assert_eq!(rendered.matches('G').count(), 1, "{rendered}");
}
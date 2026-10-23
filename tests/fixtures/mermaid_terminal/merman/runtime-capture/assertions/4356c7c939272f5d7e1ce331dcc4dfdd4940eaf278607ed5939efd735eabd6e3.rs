fn upstream_subgraph_endpoint_edges_route_to_groups_without_visible_placeholder_nodes() {
    let parsed = Engine::new()
        .parse_diagram_for_render_model_sync(SUBGRAPH_ENDPOINT_FIXTURE, ParseOptions::strict())
        .expect("upstream subgraph endpoint fixture should parse")
        .expect("upstream subgraph endpoint fixture should be detected");
    let RenderSemanticModel::Flowchart(model) = parsed.model() else {
        panic!("upstream subgraph endpoint fixture should produce a flowchart model");
    };
    for endpoint_id in ["TOP", "B1", "B2"] {
        let endpoint = model
            .nodes
            .iter()
            .find(|node| node.id == endpoint_id)
            .unwrap_or_else(|| panic!("missing endpoint node {endpoint_id:?}"));
        assert_eq!(
            endpoint.provenance,
            FlowNodeProvenance::SubgraphAnchor,
            "subgraph endpoints must carry explicit provenance"
        );
        assert!(
            model
                .subgraphs
                .iter()
                .any(|subgraph| subgraph.id == endpoint_id)
        );
    }
    let rendered = render_parsed(&parsed, &AsciiRenderOptions::ascii())
        .expect("upstream subgraph endpoint fixture should render");

    for title in ["TOP", "B1", "B2"] {
        assert_eq!(
            rendered.matches(title).count(),
            1,
            "subgraph {title:?} should own its endpoint id without a visible placeholder node:\n{rendered}"
        );
    }
    for label in ["lb1", "lb2", "lb3", "lb4", "lb5"] {
        assert!(
            rendered.contains(label),
            "subgraph endpoint routing should preserve edge label {label:?}:\n{rendered}"
        );
    }
}
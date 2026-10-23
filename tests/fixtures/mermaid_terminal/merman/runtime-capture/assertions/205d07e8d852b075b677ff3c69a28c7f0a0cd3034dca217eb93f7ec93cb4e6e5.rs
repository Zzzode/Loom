fn empty_subgraph_endpoint_keeps_rank_and_routes_to_its_perimeter() {
    let rendered = render_flowchart(concat!(
        "flowchart TD\n",
        "subgraph Empty[Empty Group]\n",
        "end\n",
        "Empty -->|handoff| Done\n",
    ));

    assert_eq!(rendered.matches("Empty Group").count(), 1, "{rendered}");
    assert!(
        rendered.contains("Done"),
        "target node should remain visible:\n{rendered}"
    );
    assert!(
        rendered.contains("handoff"),
        "edge should route from the empty group perimeter:\n{rendered}"
    );
}
fn flowchart_parser_boundary_leaving_route_reserves_long_label_extent() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TD\n",
            "Start --> Ingest\n",
            "subgraph pipe [Pipeline]\n",
            "    direction LR\n",
            "    Ingest --> Publish\n",
            "end\n",
            "Publish -->|boundaryLabelWithEnoughWidth| Success[Success]\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("boundary-leaving flowchart route should render");

    assert!(
        rendered.contains("boundaryLabelWithEnoughWidth"),
        "boundary-leaving route should not clip its long label:\n{rendered}"
    );
    for expected in ["Pipeline", "Ingest", "Publish"] {
        assert!(
            rendered.contains(expected),
            "boundary-leaving route should keep {expected:?} visible:\n{rendered}"
        );
    }
}
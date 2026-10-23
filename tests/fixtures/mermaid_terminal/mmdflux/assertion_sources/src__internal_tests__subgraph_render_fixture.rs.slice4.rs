#[test]
fn subgraph_titles_preserved_with_cross_edges() {
    let output = render_flowchart_fixture("subgraph_edges.mmd");
    assert!(
        output.contains("Input"),
        "Input title should be intact in: {}",
        output
    );
    assert!(
        output.contains("Output"),
        "Output title should be intact in: {}",
        output
    );
}
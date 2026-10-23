#[test]
fn subgraph_edges_renders_both_groups() {
    let output = render_flowchart_fixture("subgraph_edges.mmd");
    assert!(
        output.contains("Input"),
        "Should contain Input subgraph title"
    );
    assert!(output.contains("Data"), "Should contain Data node");
    assert!(output.contains("Config"), "Should contain Config node");
    assert!(output.contains("Result"), "Should contain Result node");
    assert!(output.contains("Log"), "Should contain Log node");
}
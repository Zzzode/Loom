#[test]
fn simple_subgraph_renders_title_and_nodes() {
    let output = render_flowchart_fixture("simple_subgraph.mmd");
    assert!(output.contains("Process"), "Should contain subgraph title");
    assert!(output.contains("Start"), "Should contain Start node");
    assert!(output.contains("Middle"), "Should contain Middle node");
    assert!(output.contains("End"), "Should contain End node");
}
#[test]
fn multi_subgraph_renders_both_groups() {
    let output = render_flowchart_fixture("multi_subgraph.mmd");
    assert!(output.contains("UI"), "Should contain UI node");
    assert!(output.contains("API"), "Should contain API node");
    assert!(output.contains("Server"), "Should contain Server node");
    assert!(output.contains("DB"), "Should contain DB node");
    let border_count = output.matches('┌').count();
    assert!(
        border_count >= 3,
        "Should have borders for subgraphs and nodes, got {} '┌' chars",
        border_count
    );
    assert!(output.contains("Frontend"), "Should contain Frontend title");
    assert!(output.contains("Backend"), "Should contain Backend title");
}
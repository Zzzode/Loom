#[test]
fn simple_subgraph_has_border() {
    let output = render_flowchart_fixture("simple_subgraph.mmd");
    assert!(
        output.contains('┌') && output.contains('┘'),
        "Should have box-drawing border characters"
    );
}
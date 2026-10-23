#[test]
fn simple_subgraph_ascii_mode() {
    let output = render_flowchart_fixture_ascii("simple_subgraph.mmd");
    assert!(output.contains("Process"), "ASCII: should contain title");
    assert!(output.contains("Start"), "ASCII: should contain Start");
    assert!(
        output.contains('+') && output.contains('-'),
        "ASCII mode should use +/- border characters"
    );
}
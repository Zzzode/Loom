#[test]
fn subgraph_title_embedded_in_border() {
    let output = render_flowchart_fixture("simple_subgraph.mmd");
    assert!(
        output.contains("─ Process ─") || output.contains("- Process -"),
        "Title should be embedded in border: {}",
        output
    );
}
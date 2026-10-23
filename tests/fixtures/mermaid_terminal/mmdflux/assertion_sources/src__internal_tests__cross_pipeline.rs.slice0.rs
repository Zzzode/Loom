#[test]
fn top_level_render_matches_flowchart_instance_for_subgraph_direction_mixed() {
    let input = load_fixture("subgraph_direction_mixed.mmd");
    let top_level = render_text_via_owner_pipeline("subgraph_direction_mixed.mmd");

    let instance_output =
        crate::render_diagram(&input, OutputFormat::Text, &RenderConfig::default())
            .expect("instance render should succeed");

    assert_eq!(
        top_level, instance_output,
        "top-level render() should match the flowchart instance text pipeline for subgraph fixtures"
    );
}
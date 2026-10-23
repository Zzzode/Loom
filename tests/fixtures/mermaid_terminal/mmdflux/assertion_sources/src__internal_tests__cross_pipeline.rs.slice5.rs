#[test]
fn flowchart_instance_render_is_stable_for_subgraph_direction_mixed() {
    let input = load_fixture("subgraph_direction_mixed.mmd");
    let mut baseline: Option<String> = None;

    for _ in 0..6 {
        let output = crate::render_diagram(&input, OutputFormat::Text, &RenderConfig::default())
            .expect("instance render should succeed");

        if let Some(expected) = baseline.as_ref() {
            assert_eq!(
                output, *expected,
                "flowchart instance text render should remain stable across repeated subgraph renders"
            );
        } else {
            baseline = Some(output);
        }
    }
}
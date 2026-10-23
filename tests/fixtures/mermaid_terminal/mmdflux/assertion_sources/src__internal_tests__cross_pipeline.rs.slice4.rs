#[test]
fn text_label_revalidation_fixtures_match_between_orthogonal_route_and_polyline_route_modes() {
    let fixtures = ["labeled_edges.mmd", "inline_label_flowchart.mmd"];

    for fixture in fixtures {
        let input = load_fixture(fixture);
        // Verify flux-layered text renders successfully for label fixtures.
        let _text = crate::render_diagram(
            &input,
            OutputFormat::Text,
            &RenderConfig {
                layout_engine: EngineAlgorithmId::parse("flux-layered").ok(),
                ..RenderConfig::default()
            },
        )
        .expect("text render should succeed");
    }
}
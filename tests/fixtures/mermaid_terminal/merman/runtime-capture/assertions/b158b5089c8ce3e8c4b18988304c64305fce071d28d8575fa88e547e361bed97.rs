fn compact_profile_preserves_explicit_canonical_values_from_builders() {
    let options = AsciiRenderOptions::ascii()
        .with_layout_profile(AsciiLayoutProfile::Compact)
        .with_graph_padding_x(5)
        .with_flowchart_node_label_wrap_width(40);
    let renderer = AsciiRenderer::new(options).expect("options should validate");
    let parsed = parsed_model("flowchart LR\nA[Alpha] --> B[Beta]");
    let operation = Engine::new()
        .begin_operation()
        .expect("operation context should be available");
    let report = renderer
        .render_parsed_report(
            &parsed,
            AsciiViewportPolicy::default(),
            &OperationControl::new(),
            &operation,
            merman_ascii::AsciiResourcePolicy::default(),
        )
        .expect("explicit canonical values should remain valid");
    assert_eq!(report.layout_profile, AsciiLayoutProfile::Compact);
    assert!(report.text.contains("Alpha"));
}
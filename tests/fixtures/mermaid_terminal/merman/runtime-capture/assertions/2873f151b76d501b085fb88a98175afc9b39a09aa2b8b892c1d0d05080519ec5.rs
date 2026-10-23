fn compact_profile_is_explicit_and_reported() {
    let renderer = AsciiRenderer::new(
        AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Compact),
    )
    .expect("compact options should validate");
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
        .expect("compact profile should render");
    assert_eq!(report.layout_profile, AsciiLayoutProfile::Compact);
}
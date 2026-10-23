fn empty_extent_is_reportable_without_special_sentinel_text() {
    let model = RenderSemanticModel::Timeline(Default::default());
    let operation = Engine::new()
        .begin_operation()
        .expect("operation context should be available");
    let report = AsciiRenderer::new(AsciiRenderOptions::ascii())
        .expect("options should validate")
        .render_model_report(
            &model,
            AsciiViewportPolicy::default(),
            &OperationControl::new(),
            &operation,
            merman_ascii::AsciiResourcePolicy::default(),
        )
        .expect("empty typed model should render a report");
    assert!(!report.text.contains("<empty>"));
    assert_eq!(report.emitted_extent.height, report.text.lines().count());
}
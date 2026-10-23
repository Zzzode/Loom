fn gantt_structured_text_framing_distinguishes_title_from_axis_format() {
    let mut embedded_axis = GanttDiagramRenderModel::default();
    embedded_axis.title = Some("Gantt\naxisFormat: %d".to_string());

    let mut explicit_axis = GanttDiagramRenderModel::default();
    explicit_axis.title = Some("Gantt".to_string());
    explicit_axis.axis_format = "%d".to_string();

    assert_ne!(
        render(RenderSemanticModel::Gantt(embedded_axis)),
        render(RenderSemanticModel::Gantt(explicit_axis)),
        "authored title text must not be able to forge an axisFormat field",
    );
}
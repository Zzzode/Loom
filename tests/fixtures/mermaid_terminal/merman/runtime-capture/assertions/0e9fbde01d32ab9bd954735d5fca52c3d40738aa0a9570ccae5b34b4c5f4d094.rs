fn gantt_structured_text_preserves_time_precision_and_render_end() {
    let mut model = GanttDiagramRenderModel::default();
    model.sections = vec!["Build".to_string()];
    model.tasks = vec![GanttRenderTask {
        id: "timed".to_string(),
        task: "Timed".to_string(),
        section: "Build".to_string(),
        start_ms: 1,
        end_ms: 2,
        render_end_ms: Some(3),
        ..Default::default()
    }];

    let rendered = render(RenderSemanticModel::Gantt(model));

    assert!(rendered.contains("id(bytes=5)=\"timed\""));
    assert!(
        rendered.contains("T"),
        "time-of-day precision must be visible:\n{rendered}"
    );
    assert!(rendered.contains("renderEnd="));
}
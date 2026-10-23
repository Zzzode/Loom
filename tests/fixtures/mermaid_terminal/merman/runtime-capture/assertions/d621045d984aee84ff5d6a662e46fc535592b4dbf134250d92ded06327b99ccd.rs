fn gantt_direct_model_distinguishes_fixed_and_relative_end_constraints() {
    let mut model = GanttDiagramRenderModel::default();
    model.tasks = vec![
        GanttRenderTask {
            id: "fixed".to_string(),
            task: "Fixed".to_string(),
            start_constraint: GanttTaskStartConstraint::Fixed {
                value: "2026-01-01 08:30".to_string(),
            },
            end_constraint: GanttTaskEndConstraint::Fixed {
                value: "2026-01-01 10:45".to_string(),
            },
            ..Default::default()
        },
        GanttRenderTask {
            id: "relative".to_string(),
            task: "Relative".to_string(),
            start_constraint: GanttTaskStartConstraint::PreviousTaskEnd {
                dependency_id: Some("fixed".to_string()),
            },
            end_constraint: GanttTaskEndConstraint::Duration {
                value: "2.5h".to_string(),
            },
            ..Default::default()
        },
    ];

    let rendered = render(RenderSemanticModel::Gantt(model));
    assert!(rendered.contains("start(bytes=16)=\"2026-01-01 08:30\""));
    assert!(rendered.contains("end(bytes=16)=\"2026-01-01 10:45\""));
    assert!(rendered.contains("after(bytes=5)=\"fixed\""));
    assert!(rendered.contains("duration(bytes=4)=\"2.5h\""));
}
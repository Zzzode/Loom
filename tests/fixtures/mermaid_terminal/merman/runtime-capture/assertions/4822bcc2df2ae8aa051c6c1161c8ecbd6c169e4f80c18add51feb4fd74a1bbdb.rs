fn gantt_direct_model_discloses_task_order() {
    let mut model = GanttDiagramRenderModel::default();
    model.tasks = vec![
        GanttRenderTask {
            id: "first".to_string(),
            task: "First".to_string(),
            order: 7,
            ..Default::default()
        },
        GanttRenderTask {
            id: "second".to_string(),
            task: "Second".to_string(),
            order: 3,
            ..Default::default()
        },
    ];

    let rendered = render(RenderSemanticModel::Gantt(model.clone()));
    assert!(rendered.contains("task(bytes=5)=\"First\" [id(bytes=5)=\"first\", order=7,"));
    assert!(rendered.contains("task(bytes=6)=\"Second\" [id(bytes=6)=\"second\", order=3,"));

    model.tasks[0].order = 3;
    model.tasks[1].order = 7;
    let reordered = render(RenderSemanticModel::Gantt(model));
    assert_ne!(rendered, reordered, "task order must remain recoverable");
}
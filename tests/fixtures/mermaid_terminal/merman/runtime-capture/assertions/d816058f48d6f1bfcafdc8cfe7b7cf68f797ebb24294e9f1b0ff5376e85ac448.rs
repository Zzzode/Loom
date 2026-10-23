fn timeline_structured_text_framing_distinguishes_task_text_from_events() {
    let mut embedded_event = TimelineDiagramRenderModel::default();
    embedded_event.tasks = vec![TimelineRenderTask {
        id: 1,
        section: String::new(),
        section_index: None,
        task_type: String::new(),
        task: "Task\n* Event".to_string(),
        score: 0,
        events: Vec::new(),
    }];

    let mut explicit_event = embedded_event.clone();
    explicit_event.tasks[0].task = "Task".to_string();
    explicit_event.tasks[0].events = vec!["Event".to_string()];

    assert_ne!(
        render(RenderSemanticModel::Timeline(embedded_event)),
        render(RenderSemanticModel::Timeline(explicit_event)),
        "authored task text must not be able to forge an event row",
    );
}
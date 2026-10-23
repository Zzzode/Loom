fn gantt_direct_model_rejects_duplicate_task_ids() {
    let task = GanttRenderTask {
        id: "same".to_string(),
        task: "Task".to_string(),
        section: "Build".to_string(),
        ..Default::default()
    };
    let mut model = GanttDiagramRenderModel::default();
    model.tasks = vec![task.clone(), task];

    let error = render_model(
        &RenderSemanticModel::Gantt(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate task ids cannot provide stable identity");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "gantt",
            feature: "duplicate task ids",
        }
    ));
}
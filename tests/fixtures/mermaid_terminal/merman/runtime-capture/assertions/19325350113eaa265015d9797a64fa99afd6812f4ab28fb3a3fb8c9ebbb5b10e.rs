fn timeline_render_model_wraps_long_task_and_event_text() {
    let mut model = TimelineDiagramRenderModel::default();
    model.sections = vec!["Planning".to_string()];
    model.tasks = vec![TimelineRenderTask {
            id: 0,
            section: "Planning".to_string(),
            section_index: Some(0),
            task_type: "Planning".to_string(),
            task: "Design a very long integration event stream normalization workflow that still fits readable terminal output".to_string(),
            score: 0,
            events: vec![
                "Capture every upstream payload variant without losing the important operational context".to_string(),
            ],
        }];

    let rendered = render(RenderSemanticModel::Timeline(model));

    assert_eq!(
        rendered,
        concat!(
            "direction: LR\n",
            "section(bytes=8)=\"Planning\"\n",
            "  - task(bytes=107)=\"Design a very long integration event stream normalization w\n",
            "    orkflow that still fits readable terminal output\"\n",
            "    * event(bytes=87)=\"Capture every upstream payload variant without losing the\n",
            "       important operational context\"",
        )
    );
}
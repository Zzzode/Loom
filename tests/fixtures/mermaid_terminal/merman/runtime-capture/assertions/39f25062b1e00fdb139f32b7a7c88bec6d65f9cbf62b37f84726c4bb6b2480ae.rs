fn timeline_render_model_renders_sections_tasks_and_events() {
    let mut model = TimelineDiagramRenderModel::default();
    model.title = Some("Timeline".to_string());
    model.acc_title = Some("Timeline title".to_string());
    model.acc_descr = Some("Timeline description".to_string());
    model.direction = TimelineDirection::TopDown;
    model.sections = vec!["Planning".to_string()];
    model.tasks = vec![
        TimelineRenderTask {
            id: 0,
            section: "Planning".to_string(),
            section_index: Some(0),
            task_type: "Planning".to_string(),
            task: "Design".to_string(),
            score: 0,
            events: vec!["Kickoff".to_string()],
        },
        TimelineRenderTask {
            id: 1,
            section: "Planning".to_string(),
            section_index: Some(0),
            task_type: "Planning".to_string(),
            task: "Implement".to_string(),
            score: 3,
            events: vec!["Build spec".to_string(), "Review".to_string()],
        },
    ];

    let rendered = render(RenderSemanticModel::Timeline(model));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=8)=\"Timeline\"\n",
            "accTitle(bytes=14)=\"Timeline title\"\n",
            "accDescr(bytes=20)=\"Timeline description\"\n",
            "direction: TD\n",
            "section(bytes=8)=\"Planning\"\n",
            "  - task(bytes=6)=\"Design\"\n",
            "    * event(bytes=7)=\"Kickoff\"\n",
            "  - task(bytes=9)=\"Implement\"\n",
            "    * event(bytes=10)=\"Build spec\"\n",
            "    * event(bytes=6)=\"Review\"",
        )
    );
}
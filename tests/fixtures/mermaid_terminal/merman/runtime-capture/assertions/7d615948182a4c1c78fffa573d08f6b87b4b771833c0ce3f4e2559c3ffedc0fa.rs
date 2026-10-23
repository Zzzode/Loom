fn gantt_render_model_renders_sections_tasks_and_flags() {
    let mut model = GanttDiagramRenderModel::default();
    model.title = Some("Gantt".to_string());
    model.acc_title = Some("Gantt title".to_string());
    model.acc_descr = Some("Gantt description".to_string());
    model.date_format = "YYYY-MM-DD".to_string();
    model.axis_format = "%d".to_string();
    model.sections = vec![
        "Empty".to_string(),
        "Empty".to_string(),
        "Build".to_string(),
    ];
    model.tasks = vec![GanttRenderTask {
        id: "task-1".to_string(),
        task: "Implement".to_string(),
        section: "Build".to_string(),
        task_type: "Build".to_string(),
        classes: Vec::new(),
        active: true,
        done: true,
        crit: true,
        milestone: true,
        vert: true,
        order: 0,
        start_ms: 9_223_372_036_854_775_000,
        end_ms: 9_223_372_036_854_775_001,
        render_end_ms: Some(9_223_372_036_854_775_002),
        ..Default::default()
    }];

    let rendered = render(RenderSemanticModel::Gantt(model));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=5)=\"Gantt\"\n",
            "accTitle(bytes=11)=\"Gantt title\"\n",
            "accDescr(bytes=17)=\"Gantt description\"\n",
            "dateFormat(bytes=10)=\"YYYY-MM-DD\"\n",
            "axisFormat(bytes=2)=\"%d\"\n",
            "section(bytes=5)=\"Empty\"\n",
            "section(bytes=5)=\"Empty\"\n",
            "section(bytes=5)=\"Build\"\n",
            "  - task(bytes=9)=\"Implement\" [id(bytes=6)=\"task-1\", order=0,\n",
            "    range=+292278994-08-17T07:12:55.000 -> +292278994-08-17T07:12:55.001,\n",
            "    renderEnd=+292278994-08-17T07:12:55.002, flags=milestone, active, done,\n",
            "    crit, vert]",
        )
    );
}
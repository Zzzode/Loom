fn journey_render_model_renders_actors_sections_and_scores() {
    let mut model = JourneyDiagramRenderModel::default();
    model.title = Some("Journey".to_string());
    model.acc_title = Some("Journey title".to_string());
    model.acc_descr = Some("Journey description".to_string());
    model.sections = vec!["Discovery".to_string()];
    model.tasks = vec![
        JourneyRenderTask {
            score: 5,
            score_is_nan: false,
            people: vec!["Alice".to_string(), "Bob".to_string()],
            section: "Discovery".to_string(),
            section_index: Some(0),
            task_type: "Discovery".to_string(),
            task: "Research".to_string(),
        },
        JourneyRenderTask {
            score: 3,
            score_is_nan: false,
            people: vec!["Bob".to_string()],
            section: "Discovery".to_string(),
            section_index: Some(0),
            task_type: "Discovery".to_string(),
            task: "Ship".to_string(),
        },
    ];

    let rendered = render(RenderSemanticModel::Journey(model));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=7)=\"Journey\"\n",
            "accTitle(bytes=13)=\"Journey title\"\n",
            "accDescr(bytes=19)=\"Journey description\"\n",
            "actors=[bytes=5 \"Alice\", bytes=3 \"Bob\"]\n",
            "section(bytes=9)=\"Discovery\"\n",
            "  - task(bytes=8)=\"Research\" score=5 people=[bytes=5 \"Alice\", bytes=3 \"Bob\"]\n",
            "  - task(bytes=4)=\"Ship\" score=3 people=[bytes=3 \"Bob\"]",
        )
    );
}
fn direct_models_disclose_unknown_and_unsectioned_tasks_once() {
    let mut timeline = TimelineDiagramRenderModel::default();
    timeline.sections = vec!["Known".to_string()];
    timeline.tasks = vec![
        timeline_task("Known", Some(0), "Declared"),
        timeline_task("Missing", None, "Unknown"),
        timeline_task("", None, "Loose"),
    ];
    let timeline_rendered = render_model(
        &RenderSemanticModel::Timeline(timeline),
        &AsciiRenderOptions::ascii(),
    )
    .expect("Timeline orphan tasks should remain visible");
    assert_eq!(
        timeline_rendered,
        concat!(
            "direction: LR\n",
            "section(bytes=5)=\"Known\"\n",
            "  - task(bytes=8)=\"Declared\"\n",
            "section(bytes=7)=\"Missing\" status=undeclared\n",
            "  - task(bytes=7)=\"Unknown\"\n",
            "section(bytes=0)=\"\" status=unsectioned\n",
            "  - task(bytes=5)=\"Loose\"",
        )
    );

    let mut journey = JourneyDiagramRenderModel::default();
    journey.sections = vec!["Known".to_string()];
    journey.tasks = vec![
        journey_task("Known", Some(0), "Declared"),
        journey_task("Missing", None, "Unknown"),
        journey_task("", None, "Loose"),
    ];
    let journey_rendered = render_model(
        &RenderSemanticModel::Journey(journey),
        &AsciiRenderOptions::ascii(),
    )
    .expect("Journey orphan tasks should remain visible");
    assert_eq!(
        journey_rendered,
        concat!(
            "section(bytes=5)=\"Known\"\n",
            "  - task(bytes=8)=\"Declared\" score=3\n",
            "section(bytes=7)=\"Missing\" status=undeclared\n",
            "  - task(bytes=7)=\"Unknown\" score=3\n",
            "section(bytes=0)=\"\" status=unsectioned\n",
            "  - task(bytes=5)=\"Loose\" score=3",
        )
    );
}
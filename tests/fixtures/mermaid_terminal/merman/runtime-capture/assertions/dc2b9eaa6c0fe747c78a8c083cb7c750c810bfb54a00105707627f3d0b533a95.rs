fn explicit_section_occurrences_must_be_in_bounds_and_match_the_label() {
    let mut timeline = TimelineDiagramRenderModel::default();
    timeline.sections = vec!["Known".to_string()];
    timeline.tasks = vec![timeline_task("Known", Some(1), "Out of bounds")];
    let error = render_model(
        &RenderSemanticModel::Timeline(timeline),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("out-of-bounds Timeline occurrence ownership must be rejected");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "timeline",
            feature: "section occurrence index outside declared sections",
        }
    );

    let mut journey = JourneyDiagramRenderModel::default();
    journey.sections = vec!["Known".to_string()];
    journey.tasks = vec![journey_task("Other", Some(0), "Mismatch")];
    let error = render_model(
        &RenderSemanticModel::Journey(journey),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("Journey occurrence ownership must match its section label");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "journey",
            feature: "section occurrence label mismatch",
        }
    );
}
fn duplicate_direct_section_labels_require_an_occurrence_index() {
    let mut timeline = TimelineDiagramRenderModel::default();
    timeline.sections = vec!["Repeated".to_string(), "Repeated".to_string()];
    timeline.tasks = vec![timeline_task("Repeated", None, "Ambiguous")];
    let error = render_model(
        &RenderSemanticModel::Timeline(timeline),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate Timeline labels cannot infer occurrence ownership");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "timeline",
            feature: "ambiguous section label without occurrence index",
        }
    );

    let mut journey = JourneyDiagramRenderModel::default();
    journey.sections = vec!["Repeated".to_string(), "Repeated".to_string()];
    journey.tasks = vec![journey_task("Repeated", None, "Ambiguous")];
    let error = render_model(
        &RenderSemanticModel::Journey(journey),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate Journey labels cannot infer occurrence ownership");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "journey",
            feature: "ambiguous section label without occurrence index",
        }
    );
}
fn journey_structured_text_framing_distinguishes_actor_list_items() {
    let mut one_actor = JourneyDiagramRenderModel::default();
    one_actor.actors = vec!["Alice, Bob".to_string()];

    let mut two_actors = JourneyDiagramRenderModel::default();
    two_actors.actors = vec!["Alice".to_string(), "Bob".to_string()];

    assert_ne!(
        render(RenderSemanticModel::Journey(one_actor)),
        render(RenderSemanticModel::Journey(two_actors)),
        "one authored actor containing a comma must differ from two actor values",
    );
}
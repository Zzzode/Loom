fn sequence_actor_lifecycle_consumes_shared_anchor_signals_in_mermaid_order() {
    let input = concat!(
        "sequenceDiagram\n",
        "participant A\n",
        "participant C\n",
        "create participant B\n",
        "destroy A\n",
        "C->>B: create\n",
        "A--xC: destroy\n",
    );
    let model = parse_sequence_render_model(input);

    assert_eq!(model.created_actors.get("B"), Some(&0));
    assert_eq!(model.destroyed_actors.get("A"), Some(&0));

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::unicode())
        .expect("shared lifecycle anchors must consume successive signals in Mermaid order");
    assert!(
        rendered.contains("create") && rendered.contains("destroy") && rendered.contains('×'),
        "both lifecycle signals and the destroy marker must remain visible:\n{rendered}"
    );
}
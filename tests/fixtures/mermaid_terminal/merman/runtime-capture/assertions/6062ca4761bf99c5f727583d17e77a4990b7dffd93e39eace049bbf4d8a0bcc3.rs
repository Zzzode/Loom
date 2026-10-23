fn sequence_empty_actor_order_uses_deterministic_model_order() {
    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.actor_order.clear();
    model.messages.push(message(Some("A"), Some("B"), 0));

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::ascii())
        .expect("an absent compatibility actor order should use the ordered actor map");
    assert!(rendered.contains("A") && rendered.contains("B") && rendered.contains("Hi"));
}
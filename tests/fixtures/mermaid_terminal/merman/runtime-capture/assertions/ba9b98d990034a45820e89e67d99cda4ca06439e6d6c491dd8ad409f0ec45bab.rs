fn sequence_actor_properties_are_accepted_as_omitted_metadata() {
    let mut model = basic_sequence_model();
    model
        .actors
        .get_mut("A")
        .unwrap()
        .properties
        .insert("icon".to_string(), "@clock".into());

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::ascii())
        .expect("actor presentation properties should not block ASCII rendering");
    assert!(rendered.contains('A'));
    assert!(!rendered.contains("icon"));
    assert!(!rendered.contains("@clock"));
}
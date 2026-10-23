fn class_typed_namespace_note_preserves_authored_facade_target() {
    let mut model =
        parse_class_model("classDiagram\nnamespace Domain {\n  class T\n  note for T \"same\"\n}");
    let mut facade = model.classes["T"].clone();
    facade.id = "Domain.T".to_string();
    facade.dom_id = "classDomain.T-facade".to_string();
    facade.parent = None;
    model.classes.insert(facade.id.clone(), facade);
    model
        .namespace_facade_aliases
        .insert("Domain.T".to_string(), "T".to_string());

    let resolved = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("resolved namespace note target should render");
    model.notes[0].class_id = Some("Domain.T".to_string());
    let authored = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("authored facade namespace note target should render");

    assert_ne!(resolved, authored);
    assert!(!resolved.contains("endpointRef(bytes="), "{resolved}");
    assert!(authored.contains("endpointRef(bytes=8)="), "{authored}");
    assert!(authored.contains("Domain.T"), "{authored}");
}
fn class_typed_relation_preserves_authored_facade_endpoint() {
    let mut model = parse_class_model(
        "classDiagram\nnamespace Domain {\n  class T\n}\nclass Outside\nT --> Outside",
    );
    let mut facade = model.classes["T"].clone();
    facade.id = "Domain.T".to_string();
    facade.dom_id = "classDomain.T-facade".to_string();
    facade.parent = None;
    model.classes.insert(facade.id.clone(), facade);
    model
        .namespace_facade_aliases
        .insert("Domain.T".to_string(), "T".to_string());

    let resolved = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("resolved relation endpoint should render");
    model.relations[0].id1 = "Domain.T".to_string();
    let authored = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("authored facade relation endpoint should render");

    assert_ne!(resolved, authored);
    assert!(!resolved.contains("endpointRef(bytes="), "{resolved}");
    assert!(authored.contains("endpointRef(bytes=8)="), "{authored}");
    assert!(authored.contains("Domain.T"), "{authored}");
}
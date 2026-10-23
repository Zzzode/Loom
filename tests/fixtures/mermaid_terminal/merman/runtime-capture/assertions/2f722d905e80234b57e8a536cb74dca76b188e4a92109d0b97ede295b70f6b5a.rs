fn class_external_namespace_note_summary_discloses_authored_facade_target() {
    let mut model =
        parse_class_model("classDiagram\nnamespace Domain {\n  class T\n}\nnote for T \"same\"");
    let mut facade = model.classes["T"].clone();
    facade.id = "Domain.T".to_string();
    facade.dom_id = "classDomain.T-facade".to_string();
    facade.parent = None;
    model.classes.insert(facade.id.clone(), facade);
    model
        .namespace_facade_aliases
        .insert("Domain.T".to_string(), "T".to_string());

    let resolved = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("resolved note target should render");
    model.notes[0].class_id = Some("Domain.T".to_string());
    let authored = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("qualified authored note target should render through its facade");

    assert_ne!(
        resolved, authored,
        "qualified and unqualified authored note targets must not collapse"
    );
    assert!(!resolved.contains("targetRef(bytes="), "{resolved}");
    assert!(
        authored.contains(r#"id(bytes=1)="T" targetRef(bytes=8)="Domain.T""#),
        "{authored}"
    );
}
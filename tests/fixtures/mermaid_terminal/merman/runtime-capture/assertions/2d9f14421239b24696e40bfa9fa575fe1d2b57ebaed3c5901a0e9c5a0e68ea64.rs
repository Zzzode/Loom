fn class_typed_nested_namespace_discloses_identity_when_parent_path_cannot_recover_it() {
    let render = |namespace_id: &str| {
        let mut model = parse_class_model(
            "classDiagram\nnamespace Parent {\n  namespace Child {\n    class Member\n  }\n}",
        );
        let child_key = model
            .namespaces
            .keys()
            .find(|id| id.as_str() != "Parent")
            .cloned()
            .expect("nested Child namespace should exist");
        let mut namespace = model
            .namespaces
            .shift_remove(&child_key)
            .expect("nested Child namespace should exist");
        namespace.id = namespace_id.to_string();
        namespace.label = "Child".to_string();
        namespace.dom_id = "namespace-child".to_string();
        namespace.parent = Some("Parent".to_string());
        model
            .classes
            .get_mut("Member")
            .expect("Member class should exist")
            .parent = Some(namespace.id.clone());
        model.namespaces.insert(namespace.id.clone(), namespace);

        render_class_model(&model, &AsciiRenderOptions::ascii()).unwrap_or_else(|error| {
            panic!("nested namespace {namespace_id:?} should render: {error}")
        })
    };

    let foreign = render("Foreign.Child");
    let other = render("Other.Child");

    assert_ne!(foreign, other);
    assert!(
        foreign.contains(r#"namespaceId(bytes=13)="Foreign.Child""#),
        "{foreign}"
    );
    assert!(
        other.contains(r#"namespaceId(bytes=11)="Other.Child""#),
        "{other}"
    );
}
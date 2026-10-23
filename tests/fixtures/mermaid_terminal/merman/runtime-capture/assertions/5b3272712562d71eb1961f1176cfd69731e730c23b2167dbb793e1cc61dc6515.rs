fn class_typed_root_dotted_namespace_discloses_full_identity_behind_leaf_label() {
    let render = |namespace_id: &str| {
        let mut model = parse_class_model("classDiagram\nnamespace Root {\n  class Member\n}");
        let mut namespace = model
            .namespaces
            .shift_remove("Root")
            .expect("Root namespace should exist");
        namespace.id = namespace_id.to_string();
        namespace.label = "C".to_string();
        model
            .classes
            .get_mut("Member")
            .expect("Member class should exist")
            .parent = Some(namespace.id.clone());
        model.namespaces.insert(namespace.id.clone(), namespace);

        render_class_model(&model, &AsciiRenderOptions::ascii()).unwrap_or_else(|error| {
            panic!("dotted namespace {namespace_id:?} should render: {error}")
        })
    };

    let left = render("A.C");
    let right = render("B.C");

    assert_ne!(
        left, right,
        "root dotted namespaces with the same leaf label must retain distinct identities"
    );
    assert!(left.contains(r#"namespaceId(bytes=3)="A.C""#), "{left}");
    assert!(right.contains(r#"namespaceId(bytes=3)="B.C""#), "{right}");
}
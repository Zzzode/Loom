fn class_external_namespace_note_summary_preserves_provenance_and_target_identity() {
    let render = |first_target: &str, second_target: &str| {
        let mut model = parse_class_model(
            "classDiagram\nnamespace Domain {\n  class A\n  class B\n}\nnote for A \"same\"\nnote for B \"same\"",
        );
        let mut first = model.classes.shift_remove("A").expect("A should exist");
        first.id = "\u{1b}".to_string();
        first.dom_id.clone_from(&first.id);
        first.text.clone_from(&first.id);
        let mut second = model.classes.shift_remove("B").expect("B should exist");
        second.id = r"\u{1B}".to_string();
        second.dom_id.clone_from(&second.id);
        second.text.clone_from(&second.id);
        model.classes.insert(first.id.clone(), first);
        model.classes.insert(second.id.clone(), second);

        let namespace = model
            .namespaces
            .get_mut("Domain")
            .expect("Domain namespace should exist");
        namespace.class_ids = vec!["\u{1b}".to_string(), r"\u{1B}".to_string()];
        model.notes[0].class_id = Some(first_target.to_string());
        model.notes[1].class_id = Some(second_target.to_string());

        render_class_model(&model, &AsciiRenderOptions::ascii())
            .expect("external namespace notes should render")
    };

    let authored_first = render("\u{1b}", r"\u{1B}");
    let literal_first = render(r"\u{1B}", "\u{1b}");

    assert_ne!(authored_first, literal_first);
    assert!(
        authored_first.contains(r#"note(index=1, text(bytes=4)="same")"#),
        "{authored_first}"
    );
    assert!(
        authored_first.contains(r#"id(bytes=1)="\u{1B}""#),
        "{authored_first}"
    );
    assert!(
        authored_first.contains(r#"id(bytes=6)="\\u{1B}""#),
        "{authored_first}"
    );
}
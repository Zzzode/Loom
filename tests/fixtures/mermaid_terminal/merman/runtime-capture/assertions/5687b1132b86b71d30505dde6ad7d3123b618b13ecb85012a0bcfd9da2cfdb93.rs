fn class_terminal_normalization_discloses_the_authored_box_identity() {
    let render_identity = |identity: &str| {
        let mut model = parse_class_model("classDiagram\nclass A");
        let mut class = model
            .classes
            .shift_remove("A")
            .expect("class A should exist");
        class.id = identity.to_string();
        class.text = identity.to_string();
        model.classes.insert(identity.to_string(), class);
        render_class_model(&model, &AsciiRenderOptions::ascii())
            .expect("direct class identity should render")
    };

    let control = render_identity("\u{1b}");
    let authored_escape = render_identity(r"\u{1B}");

    assert!(control.contains(r#"id(bytes=1)="\u{1B}""#), "{control}");
    assert_ne!(control, authored_escape);
}
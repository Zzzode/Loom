fn class_render_model_reconstructs_members_when_display_text_is_empty() {
    let mut model =
        parse_class_model("classDiagram\nclass Service {\n  +value\n  #compute(input) Result\n}");
    let class = model
        .classes
        .get_mut("Service")
        .expect("Service class should exist");
    let member = class
        .members
        .first_mut()
        .expect("Service should have an attribute");
    member.visibility = "+".to_string();
    member.id = "items List~T~".to_string();
    member.classifier = "$".to_string();
    member.display_text.clear();
    let method = class
        .methods
        .first_mut()
        .expect("Service should have a method");
    method.visibility = "#".to_string();
    method.id = "compute~T~".to_string();
    method.parameters = "  items: List~List~T~~  ".to_string();
    method.return_type = "  Result~List~T~~  ".to_string();
    method.classifier = "*".to_string();
    method.display_text.clear();

    let rendered = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("typed member fields should reconstruct a terminal display");

    assert!(rendered.contains("+items List<T>$"), "{rendered}");
    assert!(
        rendered.contains("#compute<T>(items: List<List<T>>) : Result<List<T>>*"),
        "{rendered}"
    );
}
fn class_render_model_preserves_independent_relationship_markers() {
    let mut model = parse_class_model("classDiagram\nclass A\nclass B\nA <|-- B");
    let aggregation = model.constants.relation_type.aggregation;
    let composition = model.constants.relation_type.composition;
    let relation = model
        .relations
        .first_mut()
        .expect("fixture should contain one relation");
    relation.relation.type1 = aggregation;
    relation.relation.type2 = composition;

    let rendered = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("independent class relationship markers should render");

    assert!(
        rendered.contains('o'),
        "aggregation marker should remain visible:\n{rendered}"
    );
    assert!(
        rendered.contains('*'),
        "composition marker should remain visible:\n{rendered}"
    );
}
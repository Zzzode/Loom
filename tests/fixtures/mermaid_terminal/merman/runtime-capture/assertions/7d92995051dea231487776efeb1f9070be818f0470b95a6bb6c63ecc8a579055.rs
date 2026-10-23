fn er_parser_attributes_render_in_entity_section() {
    let rendered = render_er(
        "erDiagram\nCUSTOMER {\n  string id PK\n  string name\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER should render");

    let id = framed_er_attribute("string", "id", &["PK"], "");
    let name = framed_er_attribute("string", "name", &[], "");
    let content_width = ["CUSTOMER".len(), id.len(), name.len()]
        .into_iter()
        .max()
        .expect("the entity fixture should have content");
    let border = format!("+{}+", "-".repeat(content_width + 2));
    assert_eq!(
        rendered,
        format!(
            "{border}\n| {:<content_width$} |\n{border}\n| {id:<content_width$} |\n| {name:<content_width$} |\n{border}\n",
            "CUSTOMER",
        )
    );
}
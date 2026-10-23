fn er_parser_spanning_relationship_routes_around_wide_intermediate_entity() {
    let rendered = render_er(
        r#"erDiagram
USER ||--o{ ORDER : places
USER {
  int id PK
  string name
  string email
}
ORDER ||--|{ ORDER_ITEM : contains
ORDER {
  int id PK
  date created_at
  string status
}
ORDER_ITEM {
  int id PK
  int quantity
  float price
}
PRODUCT ||--o{ ORDER_ITEM : "ordered in"
PRODUCT {
  int id PK
  string name
  float price
}
"#,
        &AsciiRenderOptions::unicode(),
    )
    .expect("spanning ER relationship should render around intermediate entities");

    assert!(rendered.contains("ordered in"));
    assert!(rendered.contains(&framed_er_attribute("date", "created_at", &[], "")));
    assert!(rendered.contains(&framed_er_attribute("string", "status", &[], "")));
    assert!(!rendered.contains("created_│at"));
    assert!(!rendered.contains("sta│us"));
}
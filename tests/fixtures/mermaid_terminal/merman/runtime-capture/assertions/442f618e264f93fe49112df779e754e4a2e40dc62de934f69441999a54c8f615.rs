fn class_external_namespace_note_summary_keeps_interface_endpoint() {
    let mut model = parse_class_model(concat!(
        "classDiagram\n",
        "namespace Domain {\n  class Marker\n}\n",
        "IService ()-- Service\n",
        "note for Marker \"interface note\"",
    ));
    model.relations.clear();
    model.notes[0].class_id = Some("interface0".to_string());

    let rendered = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("validated interface note target should remain in the summary");

    assert!(rendered.contains("relations:"), "{rendered}");
    assert!(
        rendered.contains(r#"note(index=1, text(bytes=14)="interface note")"#),
        "{rendered}"
    );
    assert!(
        rendered.contains(r#"id(bytes=10)="interface0""#),
        "{rendered}"
    );
    assert!(
        rendered.contains(
            r#"interfaceRef(id(bytes=10)="interface0" label(bytes=8)="IService" target(bytes=7)="Service" ordinal=1"#
        ),
        "{rendered}"
    );
}
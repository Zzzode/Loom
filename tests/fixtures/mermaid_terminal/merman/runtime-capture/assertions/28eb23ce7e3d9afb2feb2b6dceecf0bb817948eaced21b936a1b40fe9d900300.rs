fn class_parser_namespace_aliases_disclose_utf8_authored_identity() {
    let render = |root: &str| {
        render_class(
            &format!(
                "classDiagram\nnamespace {root}[\"Shared\"] {{\n  namespace API[\"Nested\"] {{\n    class C\n  }}\n}}"
            ),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("aliased namespace {root:?} should render: {error}"))
    };

    let domain = render("领域");
    let boundary = render("边界");

    assert_ne!(
        domain, boundary,
        "different authored namespace ids must not collapse behind the same visible aliases"
    );
    for expected in [
        r#"namespaceId(bytes=6)="领域""#,
        r#"namespaceId(bytes=10)="领域.API""#,
    ] {
        assert!(
            domain.contains(expected),
            "namespace identity should use UTF-8 byte framing for {expected:?}:\n{domain}"
        );
    }
    for expected in [
        r#"namespaceId(bytes=6)="边界""#,
        r#"namespaceId(bytes=10)="边界.API""#,
    ] {
        assert!(
            boundary.contains(expected),
            "nested namespace identity should retain its owning authored id {expected:?}:\n{boundary}"
        );
    }
}
fn class_parser_explicit_qualified_class_is_not_folded_into_namespace_member() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "namespace N {\n",
            "  class C\n",
            "}\n",
            "class N.C[\"Distinct\"]\n",
            "class D\n",
            "N.C --> D : calls\n",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .expect("an explicit qualified class must retain its own identity and route endpoint");

    assert_eq!(rendered.matches("│ C │").count(), 1, "{rendered}");
    assert_eq!(rendered.matches("Distinct").count(), 1, "{rendered}");
    assert!(rendered.contains("id(bytes=3)=\"N.C\""), "{rendered}");
    assert!(rendered.contains("calls"), "{rendered}");
    assert!(!rendered.contains("member(bytes=3)=\"N.C\""), "{rendered}");
}
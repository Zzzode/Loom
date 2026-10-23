fn class_parser_sibling_namespace_relation_routes_through_facades() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "namespace Left {\n  class Source\n}\n",
            "namespace Right {\n  class Target\n}\n",
            "Source \"source\" --> \"target\" Target : calls<br>async",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("sibling namespace relation should route through namespace facades");

    assert!(!rendered.contains("relations:"), "{rendered}");
    for framed_member in ["member(bytes=6)=\"Source\"", "member(bytes=6)=\"Target\""] {
        assert!(
            rendered.contains(framed_member),
            "missing framed namespace member {framed_member:?}:\n{rendered}"
        );
    }
    for expected in [
        "Left", "Right", "Source", "Target", "source", "target", "calls", "async",
    ] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
}
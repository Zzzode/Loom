fn class_parser_qualified_lollipop_target_renders_inside_its_namespace() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "namespace Domain {\n  class Service\n}\n",
            "IService ()-- Domain.Service",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("qualified lollipop target should resolve through its namespace facade alias");

    assert!(!rendered.contains("relations:"), "{rendered}");
    for expected in ["Domain", "IService", "Service"] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
}
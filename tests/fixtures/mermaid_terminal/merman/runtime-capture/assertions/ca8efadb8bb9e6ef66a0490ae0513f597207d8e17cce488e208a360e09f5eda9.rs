fn class_parser_nested_sibling_relation_routes_at_nearest_common_scope() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "namespace Platform {\n",
            "  namespace FFI {\n    class DartBinding\n  }\n",
            "  namespace Core {\n    class Renderer\n  }\n",
            "}\n",
            "DartBinding --> Renderer : invokes",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .expect("nested sibling namespace relation should route at Platform scope");

    assert!(!rendered.contains("relations:"), "{rendered}");
    for expected in [
        "Platform",
        "FFI",
        "Core",
        "DartBinding",
        "Renderer",
        "invokes",
    ] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
    for class_name in ["DartBinding", "Renderer"] {
        assert_eq!(
            rendered
                .lines()
                .filter(|line| line.contains(&format!("│ {class_name} │")))
                .count(),
            1,
            "nested namespace relation routing should render class {class_name:?} once:\n{rendered}"
        );
    }
}
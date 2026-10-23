fn class_parser_empty_namespace_does_not_force_relation_summary() {
    let rendered = render_class(
        "classDiagram
namespace Empty {
}
class A
class B
A --> B : ab",
        &AsciiRenderOptions::ascii(),
    )
    .expect("empty namespace should not affect top-level relationships");

    assert!(
        !rendered.contains("relations:"),
        "empty namespace should not force top-level relationships into summary:\n{rendered}"
    );
    assert!(
        rendered.contains("ab") && rendered.contains("+---+"),
        "top-level class relationship should keep the routed layout:\n{rendered}"
    );
    assert!(
        rendered.contains("Empty"),
        "an empty namespace should remain visible without degrading unrelated relations:\n{rendered}"
    );
}
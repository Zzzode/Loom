#[test]
fn c6_class_diagram_edge_renders_via_unified_path() {
    let output = render_class_fixture("relationships.mmd");
    for label in ["places", "contains", "authenticates"] {
        assert!(
            output.contains(label),
            "class-diagram label '{label}' missing from output\n{output}"
        );
    }
}
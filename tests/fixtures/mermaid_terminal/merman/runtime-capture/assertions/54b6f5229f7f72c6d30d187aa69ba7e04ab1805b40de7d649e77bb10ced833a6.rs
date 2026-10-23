fn class_local_semantic_fixture_covers_namespace_qualified_relationships() {
    let input = read_local_semantic_fixture("class/namespace_qualified_relationships.mmd");

    let rendered = render_class(&input, &AsciiRenderOptions::unicode())
        .expect("namespace-qualified class relationships should render");

    for (framing, authored_endpoint) in [
        ("endpointRef(bytes=24)=", "Platform.FFI.DartBinding"),
        ("endpointRef(bytes=26)=", "Platform.FFI.PythonBinding"),
        ("endpointRef(bytes=22)=", "Platform.Core.Renderer"),
    ] {
        assert!(
            rendered.contains(framing) && rendered.contains(authored_endpoint),
            "qualified relation endpoint should retain {authored_endpoint:?}:\n{rendered}"
        );
    }
    assert!(rendered.contains("Platform Layer"));
    assert!(rendered.contains("FFI"));
    assert!(rendered.contains("Core"));
    assert!(rendered.contains("DartBinding"));
    assert!(rendered.contains("PythonBinding"));
    assert!(rendered.contains("Renderer"));
    assert!(rendered.contains("relations:"), "{rendered}");
    for endpoint in [
        "member(bytes=11)=\\\"DartBinding\\\"",
        "member(bytes=13)=\\\"PythonBinding\\\"",
        "member(bytes=8)=\\\"Renderer\\\"",
    ] {
        assert!(
            rendered.contains(endpoint),
            "namespace relation summaries should preserve injective endpoint framing {endpoint:?}:\n{rendered}"
        );
    }
    assert_eq!(rendered.matches("calls").count(), 2, "{rendered}");
    assert!(
        rendered.lines().count() >= 20,
        "namespace-qualified class fixture should produce a non-trivial multi-line layout:\n{rendered}"
    );
}
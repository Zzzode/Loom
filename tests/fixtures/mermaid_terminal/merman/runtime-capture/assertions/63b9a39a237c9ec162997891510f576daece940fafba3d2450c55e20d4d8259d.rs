fn class_parser_namespace_internal_relationship_routes_inside_container() {
    let rendered = render_class(
        "classDiagram
namespace Domain {
  class Service
  class Repository
}
Service --> Repository : uses",
        &AsciiRenderOptions::ascii(),
    )
    .expect("namespace-internal class relationship should render");

    for expected in ["Domain", "Service", "Repository", "uses"] {
        assert!(
            rendered.contains(expected),
            "namespace internal relationship output should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "same-namespace class relationship should route inside the namespace container:\n{rendered}"
    );
    assert!(
        rendered.lines().any(|line| line.contains("uses")),
        "routed namespace relationship should keep its label in the container:\n{rendered}"
    );
}
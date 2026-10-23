fn class_parser_namespace_note_for_routes_inside_container() {
    let rendered = render_class(
        "classDiagram
namespace Domain {
  class Service
  note for Service \"Handles<br>requests\"
}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("namespace note should render");

    for expected in ["Domain", "Service", "Handles", "requests"] {
        assert!(
            rendered.contains(expected),
            "namespace note output should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "namespace note-for should route inside the namespace container instead of falling back to a top-level summary:\n{rendered}"
    );
    assert!(
        rendered.contains(':') || rendered.contains('.'),
        "namespace note-for should keep a dotted visual connector:\n{rendered}"
    );
    assert!(
        !rendered.contains("note0"),
        "namespace note output should not leak implementation note ids:\n{rendered}"
    );
}
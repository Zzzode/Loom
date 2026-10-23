fn class_parser_namespace_containers_render_without_relationships() {
    let rendered = render_class(
        "classDiagram
namespace Domain[\"Domain Layer\"] {
  class User
  namespace Persistence {
    class UserRepo
  }
}
class Outside",
        &AsciiRenderOptions::unicode(),
    )
    .expect("namespace containers should render");

    for expected in ["Domain Layer", "Persistence", "UserRepo", "User", "Outside"] {
        assert!(
            rendered.contains(expected),
            "namespace class output should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "namespace containers without relationships should not add a relation summary:\n{rendered}"
    );
    assert!(
        rendered.matches('┌').count() >= 4,
        "nested namespace output should contain nested terminal boxes:\n{rendered}"
    );
}
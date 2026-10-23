fn class_parser_generic_class_titles_render_type_parameters() {
    let rendered = render_class(
        "classDiagram
    direction TB
    class Repository~T~
    <<interface>> Repository~T~
    class Service~T~ {
      +get(id: String) T
    }
    class SqlRepo~T~ {
      +get(id: String) T
    }
    Repository~T~ <|.. SqlRepo~T~
    Service~T~ ..> Repository~T~ : depends",
        &AsciiRenderOptions::ascii(),
    )
    .expect("generic class diagram should render");

    for expected in ["Repository<T>", "Service<T>", "SqlRepo<T>"] {
        assert!(
            rendered.contains(expected),
            "generic class title should keep {expected:?} visible:\n{rendered}"
        );
    }
}
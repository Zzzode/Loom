fn class_parser_authored_none_endpoint_labels_are_not_absence_sentinels() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nA \"none\" --> \"NONE\" B",
        &AsciiRenderOptions::ascii(),
    )
    .expect("authored endpoint labels should render");

    for label in ["none", "NONE"] {
        assert!(
            rendered.contains(label),
            "authored endpoint label {label:?} should remain visible:\n{rendered}"
        );
    }

    let absent = render_class(
        "classDiagram\nclass A\nclass B\nA --> B",
        &AsciiRenderOptions::ascii(),
    )
    .expect("an absent endpoint label should render");
    let authored_empty = render_class(
        "classDiagram\nclass A\nclass B\nA \"\" --> B",
        &AsciiRenderOptions::ascii(),
    )
    .expect("an authored empty endpoint label should render");
    assert_ne!(absent, authored_empty);
}
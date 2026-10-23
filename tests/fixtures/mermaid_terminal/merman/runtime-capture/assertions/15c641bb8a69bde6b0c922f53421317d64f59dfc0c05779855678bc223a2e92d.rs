fn class_relation_summary_frames_multiline_endpoint_labels_injectively() {
    let source = |endpoint_label: &str| {
        format!(
            concat!(
                "classDiagram\n",
                "direction BT\n",
                "class A\n",
                "class B\n",
                "class C\n",
                "A \"{}\" --> B : ab\n",
                "B --> A : ba\n",
                "A --> C : ac\n",
                "C --> A : ca\n",
                "B --> C : bc\n",
                "C --> B : cb",
            ),
            endpoint_label
        )
    };
    let literal_slash = render_class(&source("a/b"), &AsciiRenderOptions::ascii())
        .expect("literal slash endpoint label should render through summary fallback");
    let authored_break = render_class(&source("a<br>b"), &AsciiRenderOptions::ascii())
        .expect("multiline endpoint label should render through summary fallback");
    let authored_empty = render_class(&source(""), &AsciiRenderOptions::ascii())
        .expect("empty endpoint label should render through summary fallback");

    assert!(literal_slash.contains("relations:"), "{literal_slash}");
    assert!(authored_break.contains("relations:"), "{authored_break}");
    assert!(authored_empty.contains("relations:"), "{authored_empty}");
    assert!(
        literal_slash.contains("endpoint1=[bytes=3 \"a/b\"] -->"),
        "literal slash identity should remain framed on the semantic source:\n{literal_slash}"
    );
    assert!(
        authored_break.contains("endpoint1=[bytes=1 \"a\", bytes=1 \"b\",")
            && authored_break.contains("authored(bytes=6)=")
            && authored_break.contains("<br>")
            && authored_break.contains("] -->"),
        "authored line boundaries and source bytes should remain framed:\n{authored_break}"
    );
    assert!(
        authored_empty.contains("endpoint1=[bytes=0 \"\",")
            && authored_empty.contains("authored(bytes=0)=")
            && authored_empty.contains("] -->"),
        "authored empty endpoint labels should remain framed:\n{authored_empty}"
    );
    assert_ne!(literal_slash, authored_break);
    assert_ne!(literal_slash, authored_empty);
}
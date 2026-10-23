fn class_parser_four_layer_relation_layout_uses_iterative_sweep_order() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "class A0\nclass A1\nclass A2\n",
            "class B0\nclass B1\nclass B2\n",
            "class C0\nclass C1\nclass C2\n",
            "class D0\nclass D1\nclass D2\n",
            "C0 --> D0 : c0d0\n",
            "A1 --> B1 : a1b1\n",
            "A2 --> B0 : a2b0\n",
            "A1 --> B0 : a1b0\n",
            "B2 --> C2 : b2c2\n",
            "B2 --> C0 : b2c0\n",
            "C2 --> D1 : c2d1\n",
            "C2 --> D0 : c2d0\n",
            "A0 --> B1 : a0b1\n",
            "A0 --> B2 : a0b2\n",
            "C1 --> D2 : c1d2\n",
            "B2 --> C1 : b2c1",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("four-layer class relation topology should render");

    assert!(
        !rendered.contains("relations:"),
        "four-layer class topology should use routed layout, not summary:\n{rendered}"
    );
    for expected in [
        "a0b1", "a0b2", "a1b0", "a1b1", "a2b0", "b2c0", "b2c1", "b2c2", "c0d0", "c1d2", "c2d0",
        "c2d1",
    ] {
        assert!(
            rendered.contains(expected),
            "routed four-layer class topology should keep {expected:?} visible:\n{rendered}"
        );
    }
}
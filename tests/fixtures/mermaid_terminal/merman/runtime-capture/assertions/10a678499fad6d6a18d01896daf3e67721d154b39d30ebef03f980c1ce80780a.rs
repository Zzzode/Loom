fn flowchart_parser_multibyte_reference_labels_render_readably() {
    let cases = [
        (
            "graph LR\nA[Café]-->|résumé|B[Über]",
            ["Café", "résumé", "Über"],
        ),
        (
            "graph LR\nA[Γειά]-->|ετικέτα|B[Κόσμος]",
            ["Γειά", "ετικέτα", "Κόσμος"],
        ),
        (
            "graph LR\nA[Привет]-->|метка|B[Мир]",
            ["Привет", "метка", "Мир"],
        ),
    ];

    for (input, expected_labels) in cases {
        let rendered = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();

        for label in expected_labels {
            assert!(
                rendered.contains(label),
                "missing {label:?} in rendered fixture:\n{rendered}"
            );
        }
        assert_rectangular_char_grid(&rendered);
    }
}
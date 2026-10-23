fn er_parser_preserves_entity_declaration_order() {
    let input = "erDiagram\nZETA\nALPHA\nMIDDLE";
    let model = parse_er_model(input);
    assert!(
        model
            .entities
            .values()
            .map(|entity| entity.label.as_str())
            .eq(["ZETA", "ALPHA", "MIDDLE"])
    );

    let rendered = render_er(input, &AsciiRenderOptions::ascii())
        .expect("declaration-ordered ER entities should render");
    assert!(
        first_line_index_containing(&rendered, "| ZETA |")
            < first_line_index_containing(&rendered, "| ALPHA |")
    );
    assert!(
        first_line_index_containing(&rendered, "| ALPHA |")
            < first_line_index_containing(&rendered, "| MIDDLE |")
    );
}
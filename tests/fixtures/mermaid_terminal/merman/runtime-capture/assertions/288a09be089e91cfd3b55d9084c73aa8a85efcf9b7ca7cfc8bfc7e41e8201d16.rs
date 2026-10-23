fn er_parser_independent_relationship_pairs_render_without_shared_summary_state() {
    let options = AsciiRenderOptions::ascii();

    let rendered = render_er(
        "erDiagram\nCUSTOMER ||--o{ ORDER : places\nINVOICE ||--|| PAYMENT : captures",
        &options,
    )
    .expect("independent ER relationship pairs should render separately");

    for expected in [
        "CUSTOMER", "ORDER", "INVOICE", "PAYMENT", "places", "captures",
    ] {
        assert!(
            rendered.contains(expected),
            "independent ER relationship pairs should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "independent ER relationship pairs should remain routed without shared summary state:\n{rendered}"
    );
}
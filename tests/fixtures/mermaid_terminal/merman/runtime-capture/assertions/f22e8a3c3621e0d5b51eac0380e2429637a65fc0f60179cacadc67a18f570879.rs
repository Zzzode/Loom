fn sequence_rect_par_over_empty_sections_render_visible_lifeline_rows() {
    let mut cases = Vec::new();

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model
        .messages
        .push(message(None, None, LINETYPE_RECT_START));
    model.messages.push(empty_message(LINETYPE_RECT_END));
    cases.push(("rect", model));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model
        .messages
        .push(message(None, None, LINETYPE_PAR_OVER_START));
    model.messages.push(empty_message(LINETYPE_PAR_END));
    cases.push(("par_over", model));

    for (keyword, model) in cases {
        let rendered = render_sequence_model(&model, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|err| panic!("empty {keyword} should render: {err}"));
        assert!(
            rendered.contains(&format!("{keyword} Hi")),
            "empty {keyword} should retain its frame title:\n{rendered}"
        );
        assert!(
            rendered.lines().any(|line| line.starts_with('│')),
            "empty {keyword} should retain a visible lifeline row:\n{rendered}"
        );
    }
}
fn sequence_empty_control_block_sections_render_visible_lifeline_rows() {
    let mut cases = Vec::new();

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model
        .messages
        .push(message(None, None, LINETYPE_LOOP_START));
    model.messages.push(empty_message(LINETYPE_LOOP_END));
    cases.push(("loop", model, None));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.messages.push(message(None, None, LINETYPE_ALT_START));
    model.messages.push(message(None, None, LINETYPE_ALT_ELSE));
    model.messages.push(empty_message(LINETYPE_ALT_END));
    cases.push(("alt", model, Some("else")));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.messages.push(message(None, None, LINETYPE_PAR_START));
    model.messages.push(message(None, None, LINETYPE_PAR_AND));
    model.messages.push(empty_message(LINETYPE_PAR_END));
    cases.push(("par", model, Some("and")));

    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model
        .messages
        .push(message(None, None, LINETYPE_CRITICAL_START));
    model
        .messages
        .push(message(None, None, LINETYPE_CRITICAL_OPTION));
    model.messages.push(empty_message(LINETYPE_CRITICAL_END));
    cases.push(("critical", model, Some("option")));

    for (keyword, model, separator) in cases {
        let rendered = render_sequence_model(&model, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|err| panic!("empty {keyword} should render: {err}"));
        assert!(
            rendered.contains(&format!("{keyword} Hi")),
            "empty {keyword} should retain its frame title:\n{rendered}"
        );
        if let Some(separator) = separator {
            assert!(
                rendered.contains(&format!("{separator} Hi")),
                "empty {keyword} should retain its empty section separator:\n{rendered}"
            );
        }
        let expected_sections = usize::from(separator.is_some()) + 1;
        assert!(
            rendered
                .lines()
                .filter(|line| line.starts_with('│'))
                .count()
                >= expected_sections,
            "every empty {keyword} section should retain a visible lifeline row:\n{rendered}"
        );
    }
}
fn er_terminal_width_profile_preserves_complex_graphemes_and_ambiguous_width() {
    let mut model = parse_er_model("erDiagram\nA");
    let mut entity = model
        .entities
        .shift_remove("A")
        .expect("entity A should exist");
    entity.id = "👩‍💻·".to_string();
    entity.label = "👩‍💻·".to_string();
    entity.alias.clear();
    model.entities.insert("👩‍💻·".to_string(), entity);

    let unicode = render_er_model(
        &model,
        &AsciiRenderOptions::ascii().with_terminal_width_profile(TerminalWidthProfile::Unicode),
    )
    .expect("ER entity should render with Unicode terminal widths");
    let cjk = render_er_model(
        &model,
        &AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Cjk),
    )
    .expect("ER entity should render with CJK terminal widths");

    assert_eq!(unicode, "+-----+\n| 👩‍💻· |\n+-----+\n");
    assert_eq!(cjk, "+------+\n| 👩‍💻· |\n+------+\n");
    assert!(
        !cjk.contains(['┌', '─', '┐', '│', '└', '┘']),
        "CJK width profiles must use single-cell structural glyphs: {cjk}"
    );
}
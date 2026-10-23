fn class_terminal_width_profile_preserves_complex_graphemes_and_ambiguous_width() {
    let mut model = parse_class_model("classDiagram\nclass A");
    let mut class = model
        .classes
        .shift_remove("A")
        .expect("class A should exist");
    class.id = "👩‍💻·".to_string();
    class.text = "👩‍💻·".to_string();
    model.classes.insert("👩‍💻·".to_string(), class);

    let unicode = render_class_model(
        &model,
        &AsciiRenderOptions::ascii().with_terminal_width_profile(TerminalWidthProfile::Unicode),
    )
    .expect("class should render with Unicode terminal widths");
    let cjk = render_class_model(
        &model,
        &AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Cjk),
    )
    .expect("class should render with CJK terminal widths");

    assert_eq!(unicode, "+-----+\n| 👩‍💻· |\n+-----+\n");
    assert_eq!(cjk, "+------+\n| 👩‍💻· |\n+------+\n");
    assert!(
        !cjk.contains(['┌', '─', '┐', '│', '└', '┘']),
        "CJK width profiles must use single-cell structural glyphs: {cjk}"
    );
}
fn cjk_width_profile_changes_only_the_measured_extent() {
    let source = "flowchart TD\nA[\"A·B C\"]";
    let model = parse_model(source);
    let unicode = render_model(&model, &AsciiRenderOptions::unicode())
        .expect("Unicode profile should render the ambiguous-width fixture");
    let cjk = render_model(
        &model,
        &AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Cjk),
    )
    .expect("CJK profile should render the ambiguous-width fixture");

    assert!(unicode.contains("A·B"), "label prefix was lost:\n{unicode}");
    assert!(unicode.contains('C'), "label suffix was lost:\n{unicode}");
    assert!(cjk.contains("A·B"), "label prefix was lost:\n{cjk}");
    assert!(cjk.contains('C'), "label suffix was lost:\n{cjk}");
    assert_rectangular_terminal_grid(&unicode);
    assert_rectangular_terminal_grid_with_profile(&cjk, TerminalWidthProfile::Cjk);

    let (unicode_width, unicode_height) =
        support::terminal_extent_with_profile(&unicode, TerminalWidthProfile::Unicode);
    let (cjk_width, cjk_height) =
        support::terminal_extent_with_profile(&cjk, TerminalWidthProfile::Cjk);
    assert!(
        cjk_width >= unicode_width && cjk_height >= unicode_height,
        "CJK width changes wrapping/extent only in the selected profile:\nunicode:\n{unicode}\ncjk:\n{cjk}"
    );
}
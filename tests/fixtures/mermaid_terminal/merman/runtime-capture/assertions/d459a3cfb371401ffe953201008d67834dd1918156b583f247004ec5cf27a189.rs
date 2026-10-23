fn flowchart_node_label_wrapping_uses_the_selected_width_profile() {
    let input = "flowchart TD\nA[\"A·B C\"]";
    let unicode = render_flowchart(
        input,
        &AsciiRenderOptions::unicode().with_flowchart_node_label_wrap_width(5),
    )
    .expect("Unicode-width label should render");
    let cjk = render_flowchart(
        input,
        &AsciiRenderOptions::unicode()
            .with_terminal_width_profile(merman_ascii::TerminalWidthProfile::Cjk)
            .with_flowchart_node_label_wrap_width(5),
    )
    .expect("CJK-width label should render");

    assert!(unicode.contains("A·B C"), "{unicode}");
    assert!(cjk.contains("A·B"), "{cjk}");
    assert!(cjk.contains('C'), "{cjk}");
    assert!(
        cjk.lines().count() > unicode.lines().count(),
        "the ambiguous-width character should force an extra CJK label row:\nunicode:\n{unicode}\ncjk:\n{cjk}"
    );
}
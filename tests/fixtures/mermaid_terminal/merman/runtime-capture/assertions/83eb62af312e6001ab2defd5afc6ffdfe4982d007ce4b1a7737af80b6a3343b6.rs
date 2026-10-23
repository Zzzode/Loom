fn flowchart_layout_uses_the_selected_ambiguous_width_profile() {
    let source = "flowchart LR\nA[\"A·B\"]";
    let unicode = render(
        source,
        &AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Unicode),
    );
    let cjk = render(
        source,
        &AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Cjk),
    );

    let unicode_border = unicode
        .lines()
        .find(|line| line.contains('┌'))
        .expect("Unicode output should contain a node border");
    let cjk_border = cjk
        .lines()
        .find(|line| line.contains('+'))
        .expect("CJK output should use a single-cell ASCII node border");

    assert_eq!(
        UnicodeWidthStr::width_cjk(cjk_border),
        UnicodeWidthStr::width(unicode_border) + 1,
        "Unicode:\n{unicode}\nCJK:\n{cjk}"
    );
}
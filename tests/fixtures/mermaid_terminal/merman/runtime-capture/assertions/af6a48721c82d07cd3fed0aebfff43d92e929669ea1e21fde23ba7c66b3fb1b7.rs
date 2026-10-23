fn structured_text_html_escapes_authored_markup_after_normalization() {
    let model = git_graph_with_authored_text("<node & title>\u{1b}");
    let options = AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html);

    let rendered = render_model(&model, &options).expect("git graph renders");

    assert!(!rendered.contains("<node & title>"), "{rendered:?}");
    assert!(
        rendered.contains("&lt;node &amp; title&gt;"),
        "{rendered:?}"
    );
    assert!(rendered.contains(r"\u{1B}"), "{rendered:?}");
    assert!(!rendered.contains('\u{1b}'), "{rendered:?}");
}
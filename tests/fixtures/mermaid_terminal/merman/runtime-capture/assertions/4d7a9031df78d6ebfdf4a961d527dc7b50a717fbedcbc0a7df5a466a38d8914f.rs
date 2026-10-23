fn structured_text_visibly_escapes_terminal_and_bidi_controls() {
    let authored = "before\u{1b}]8;;https://example.invalid\u{7}link\u{9b}after\u{202e}";
    let model = git_graph_with_authored_text(authored);

    let rendered = render_model(&model, &AsciiRenderOptions::ascii()).expect("git graph renders");

    for control in ['\u{1b}', '\u{7}', '\u{9b}', '\u{202e}'] {
        assert!(
            !rendered.contains(control),
            "raw control {control:?} leaked: {rendered:?}"
        );
    }
    assert!(rendered.contains(r"\u{1B}"), "{rendered:?}");
    assert!(rendered.contains(r"\u{7}"), "{rendered:?}");
    assert!(rendered.contains(r"\u{9B}"), "{rendered:?}");
    assert!(rendered.contains(r"\u{202E}"), "{rendered:?}");
}
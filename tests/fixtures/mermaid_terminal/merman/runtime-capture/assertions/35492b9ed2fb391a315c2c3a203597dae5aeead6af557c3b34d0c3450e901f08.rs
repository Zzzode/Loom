fn every_current_family_preserves_complete_graphemes_in_both_width_profiles() {
    let authored = "Cafe\u{301} 👩‍💻 🇺🇸";

    for (family, source) in CURRENT_ASCII_FAMILY_SOURCES {
        let model = model_with_visible_text(family, source, authored);
        for charset_options in [AsciiRenderOptions::ascii(), AsciiRenderOptions::unicode()] {
            for profile in [TerminalWidthProfile::Unicode, TerminalWidthProfile::Cjk] {
                let options = charset_options.with_terminal_width_profile(profile);
                let rendered = render_model(&model, &options)
                    .unwrap_or_else(|error| panic!("{family}/{profile:?} failed: {error}"));

                assert!(
                    rendered.contains(authored),
                    "{family}/{profile:?} split or dropped a grapheme:\n{rendered}"
                );
            }
        }
    }
}
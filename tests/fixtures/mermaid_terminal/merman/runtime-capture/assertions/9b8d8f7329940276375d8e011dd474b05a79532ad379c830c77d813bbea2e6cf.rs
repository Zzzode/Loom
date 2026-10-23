fn sequence_golden_unicode_fixtures_match_upstream() {
    assert_copied_sequence_fixtures("sequence", &AsciiRenderOptions::unicode());
}
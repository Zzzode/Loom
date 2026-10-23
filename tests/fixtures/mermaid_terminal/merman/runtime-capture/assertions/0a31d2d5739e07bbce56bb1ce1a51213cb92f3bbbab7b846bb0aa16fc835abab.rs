fn sequence_golden_ascii_fixtures_match_upstream() {
    assert_copied_sequence_fixtures("sequence-ascii", &AsciiRenderOptions::ascii());
}
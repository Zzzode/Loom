fn compact_sequence_ansi16_preserves_plain_geometry_for_control_and_lifecycle_rows() {
    let input = read_local_semantic_fixture("sequence/dense_control_rows.mmd");
    let plain_options =
        AsciiRenderOptions::ascii().with_layout_profile(merman_ascii::AsciiLayoutProfile::Compact);
    let plain = render_sequence(&input, &plain_options).unwrap();
    let ansi16 = render_sequence(
        &input,
        &plain_options.with_color_mode(AsciiColorMode::Ansi16),
    )
    .unwrap();

    assert_eq!(strip_ansi(&ansi16), plain);
}
fn auto_requires_a_positive_viewport_and_string_helpers_cannot_supply_one() {
    let model = parse_model("flowchart LR\nA --> B");
    let options = AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Auto);
    for viewport in [
        AsciiViewportPolicy::unrestricted(),
        AsciiViewportPolicy::with_max_width(0),
    ] {
        assert!(matches!(
            render_model_report(&model, &options, viewport),
            Err(AsciiError::InvalidOption {
                field: "ascii_viewport.max_width",
                ..
            })
        ));
    }
    assert!(matches!(
        render_model(&model, &options),
        Err(AsciiError::InvalidOption {
            field: "ascii_viewport.max_width",
            ..
        })
    ));
}
fn state_layout_does_not_inherit_the_flowchart_compact_profile() {
    let input = concat!(
        "stateDiagram-v2\n",
        "state Parent {\n",
        "  A --> B: advance\n",
        "}\n",
        "Parent --> Done: finish",
    );
    render_state(input, &AsciiRenderOptions::ascii())
        .expect("canonical state layout should render");
    let error = render_state(
        input,
        &AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Compact),
    )
    .expect_err("State must reject the unadmitted Flowchart compact profile");

    assert_eq!(
        error,
        AsciiError::InvalidOption {
            field: "layout_profile",
            message: "is not admitted for this diagram family",
        }
    );
}
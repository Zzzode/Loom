fn explicit_control_success_matches_default_test_operation_output() {
    let model = parse_model("flowchart LR\n  A[Start] --> B[Finish]\n");
    let options = AsciiRenderOptions::ascii();
    let expected = render_model(&model, &options).expect("default test operation should succeed");
    let actual = render_controlled_model(
        &model,
        &options,
        &OperationControl::new(),
        &operation_context(),
        AsciiResourcePolicy::default(),
    )
    .expect("controlled render should succeed");

    assert_eq!(actual, expected);
}
fn cancelled_operation_is_distinct_from_resource_exhaustion() {
    let model = parse_model("flowchart LR\n  A --> B\n  B --> C\n");
    let control = OperationControl::new();
    control.cancel();

    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &control,
        &operation_context(),
        AsciiResourcePolicy::default(),
    )
    .expect_err("cancelled operation must not produce output");

    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == OperationPhase::Admission
                && cancelled.reason == merman_core::CancelReason::Requested
    ));
}
fn xychart_resource_admission_observes_cancellation_before_layout_work_failure() {
    let model = parse_model("xychart\nx-axis [A]\ny-axis 0 --> 1\nbar [1]\n");
    let control = OperationControl::new();
    control.cancel_after_checkpoints(3);

    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &control,
        &operation_context(),
        AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, 1)
            .expect("one work unit is a valid ASCII resource limit"),
    )
    .expect_err("scheduled cancellation must precede XYChart work exhaustion");

    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == OperationPhase::Layout
                && cancelled.reason == merman_core::CancelReason::Requested
    ));
}
fn class_resource_admission_observes_cancellation_before_layout_work_failure() {
    let model = parse_model("classDiagram\nclass A\n");
    let control = OperationControl::new();
    control.cancel_after_checkpoints(2);

    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &control,
        &operation_context(),
        AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, 1)
            .expect("one work unit is a valid ASCII resource limit"),
    )
    .expect_err("scheduled cancellation must precede class work exhaustion");

    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == OperationPhase::Semantic
                && cancelled.reason == merman_core::CancelReason::Requested
    ));
}
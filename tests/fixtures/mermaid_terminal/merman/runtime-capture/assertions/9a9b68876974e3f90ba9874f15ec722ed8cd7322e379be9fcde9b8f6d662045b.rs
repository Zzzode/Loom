fn state_projection_observes_cancellation_during_semantic_work() {
    let model = parse_model("stateDiagram-v2\n  [*] --> Active\n  Active --> [*]\n");
    let control = OperationControl::new();
    control.cancel_after_checkpoints(2);

    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &control,
        &operation_context(),
        AsciiResourcePolicy::default(),
    )
    .expect_err("state projection should observe scheduled cancellation");

    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == OperationPhase::Semantic
                && cancelled.reason == merman_core::CancelReason::Requested
    ));
}
fn flowchart_projection_observes_cancellation_during_semantic_work() {
    let model = parse_model("flowchart LR\n  A --> B\n  B --> C\n");
    let control = OperationControl::new();
    control.cancel_after_checkpoints(2);

    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &control,
        &operation_context(),
        AsciiResourcePolicy::default(),
    )
    .expect_err("flowchart projection should observe scheduled cancellation");

    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == OperationPhase::Semantic
                && cancelled.reason == merman_core::CancelReason::Requested
    ));
}
fn sequence_projection_observes_cancellation_at_semantic_checkpoint() {
    let model =
        parse_model("sequenceDiagram\nparticipant A\nparticipant B\nA->>B: one\nB-->>A: two\n");
    let control = OperationControl::new();
    control.cancel_after_checkpoints(2);

    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &control,
        &operation_context(),
        AsciiResourcePolicy::default(),
    )
    .expect_err("sequence projection should observe scheduled cancellation");

    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == OperationPhase::Semantic
                && cancelled.reason == merman_core::CancelReason::Requested
    ));
}
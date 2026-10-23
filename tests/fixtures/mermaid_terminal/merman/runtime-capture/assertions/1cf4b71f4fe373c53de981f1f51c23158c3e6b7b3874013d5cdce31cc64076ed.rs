fn structured_text_resource_admission_observes_cancellation_before_layout_work_failure() {
    let resources = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, 1)
        .expect("one work unit is a valid ASCII resource limit");

    for (diagram_type, model) in structured_text_models() {
        let resource_error = render_controlled_model(
            &model,
            &AsciiRenderOptions::ascii(),
            &OperationControl::new(),
            &operation_context(),
            resources,
        )
        .expect_err("the constrained family model should exceed one layout work unit");
        assert!(
            matches!(
                resource_error,
                AsciiError::ResourceLimitExceeded(details)
                    if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
            ),
            "{diagram_type} should reach the expected layout-work ceiling without cancellation: {resource_error:?}",
        );

        let control = OperationControl::new();
        control.cancel_after_checkpoints(2);
        let cancelled = render_controlled_model(
            &model,
            &AsciiRenderOptions::ascii(),
            &control,
            &operation_context(),
            resources,
        )
        .expect_err("scheduled cancellation must precede structured-text work exhaustion");
        assert!(
            matches!(
                cancelled,
                AsciiError::Cancelled(details)
                    if details.phase == OperationPhase::Layout
                        && details.reason == merman_core::CancelReason::Requested
            ),
            "{diagram_type} should report layout cancellation before the resource ceiling: {cancelled:?}",
        );
    }
}
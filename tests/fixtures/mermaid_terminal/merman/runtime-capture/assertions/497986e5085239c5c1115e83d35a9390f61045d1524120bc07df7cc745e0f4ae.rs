fn tree_view_recursive_emit_observes_operation_cancellation() {
    let mut node = tree_node(32, 31, "leaf", Vec::new());
    for depth in (0..32).rev() {
        node = tree_node(depth, depth - 1, &format!("node-{depth}"), vec![node]);
    }
    let model = TreeViewDiagramRenderModel {
        acc_title: None,
        acc_descr: None,
        title: None,
        root: tree_node(100, -1, "/", vec![node]),
    };

    // Schedule beyond the complete iterative validation pass so this remains an Emit traversal
    // regression rather than a validation-cancellation test.
    let error = render_with_scheduled_cancellation(RenderSemanticModel::TreeView(model), 256);
    assert!(
        matches!(
            error,
            AsciiError::Cancelled(cancelled)
                if cancelled.phase == merman_core::OperationPhase::Emit
        ),
        "expected Emit cancellation, got {error:?}"
    );
}
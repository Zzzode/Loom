fn git_graph_summary_loop_observes_operation_cancellation() {
    let model = GitGraphRenderModel {
        diagram_type: "gitGraph".to_string(),
        commits: (0..32)
            .map(|seq| GitGraphCommitRenderModel {
                id: format!("c{seq}"),
                message: format!("commit {seq}"),
                seq,
                commit_type: 0,
                tags: Vec::new(),
                parents: Vec::new(),
                branch: "main".to_string(),
                custom_type: None,
                custom_id: None,
            })
            .collect(),
        branches: vec![GitGraphBranchRenderModel {
            name: "main".to_string(),
        }],
        current_branch: "main".to_string(),
        direction: "TB".to_string(),
        title: None,
        acc_title: None,
        acc_descr: None,
        warning_facts: Vec::new(),
    };

    // Commit-type validation now checks each commit before the summary is emitted. Advance past
    // those semantic checks so this fixture continues to exercise the Emit loop itself.
    let successful_checkpoints = model.commits.len() + 4;
    let error = render_with_scheduled_cancellation(
        RenderSemanticModel::GitGraph(model),
        successful_checkpoints,
    );
    assert!(matches!(
        error,
        AsciiError::Cancelled(cancelled)
            if cancelled.phase == merman_core::OperationPhase::Emit
    ));
}
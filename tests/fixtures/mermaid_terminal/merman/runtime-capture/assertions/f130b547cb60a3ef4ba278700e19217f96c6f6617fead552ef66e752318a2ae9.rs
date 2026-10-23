fn git_graph_render_model_rejects_unknown_commit_types() {
    for commit_type in [98, 99] {
        let model = GitGraphRenderModel {
            diagram_type: "gitGraph".to_string(),
            commits: vec![GitGraphCommitRenderModel {
                id: "c0".to_string(),
                message: "unknown".to_string(),
                seq: 0,
                commit_type,
                tags: Vec::new(),
                parents: Vec::new(),
                branch: "main".to_string(),
                custom_type: None,
                custom_id: None,
            }],
            branches: Vec::new(),
            current_branch: "main".to_string(),
            direction: "TB".to_string(),
            title: None,
            acc_title: None,
            acc_descr: None,
            warning_facts: Vec::new(),
        };

        let error = render_model(
            &RenderSemanticModel::GitGraph(model),
            &AsciiRenderOptions::ascii(),
        )
        .expect_err("unknown direct-model commit types must not disappear");
        assert_eq!(
            error,
            AsciiError::UnsupportedFeature {
                diagram_type: "gitGraph",
                feature: "unknown commit types",
            }
        );
    }
}
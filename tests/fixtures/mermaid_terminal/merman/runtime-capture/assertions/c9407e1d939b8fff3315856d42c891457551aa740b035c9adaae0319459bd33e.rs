fn git_graph_render_model_renders_branches_commits_and_warnings() {
    let model = GitGraphRenderModel {
        diagram_type: "gitGraph".to_string(),
        commits: vec![GitGraphCommitRenderModel {
            id: "c0".to_string(),
            message: "init".to_string(),
            seq: 0,
            commit_type: 2,
            tags: vec!["v1".to_string()],
            parents: vec!["seed".to_string()],
            branch: "main".to_string(),
            custom_type: Some(7),
            custom_id: Some(true),
        }],
        branches: vec![
            GitGraphBranchRenderModel {
                name: "main".to_string(),
            },
            GitGraphBranchRenderModel {
                name: "feature".to_string(),
            },
        ],
        current_branch: "main".to_string(),
        direction: "TB".to_string(),
        title: Some("Repository history".to_string()),
        acc_title: Some("Git title".to_string()),
        acc_descr: Some("Git description".to_string()),
        warning_facts: vec![DiagramWarningFact::new(
            GIT_GRAPH_DUPLICATE_COMMIT_WARNING_RULE_ID,
            "duplicate head",
        )],
    };

    let rendered = render(RenderSemanticModel::GitGraph(model));

    assert_eq!(
        rendered,
        concat!(
            "gitGraph direction(bytes=2)=\"TB\" current(bytes=4)=\"main\"\n",
            "title(bytes=18)=\"Repository history\"\n",
            "accTitle(bytes=9)=\"Git title\"\n",
            "accDescr(bytes=15)=\"Git description\"\n",
            "branches=[bytes=4 \"main\", bytes=7 \"feature\"]\n",
            "  - seq=0 branch(bytes=4)=\"main\" id(bytes=2)=\"c0\" kind=highlight message(bytes=4)=\"init\" tags=[bytes=2 \"v1\"] parents=[bytes=4 \"seed\"] typeOverride=7 idSource=explicit\n",
            "warnings:\n",
            "  - message(bytes=14)=\"duplicate head\"",
        )
    );
}
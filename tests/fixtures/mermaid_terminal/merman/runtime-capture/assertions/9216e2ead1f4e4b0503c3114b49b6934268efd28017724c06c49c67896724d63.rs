fn git_graph_branch_and_commit_identity_fields_are_length_framed() {
    let base = GitGraphRenderModel {
        diagram_type: "gitGraph".to_string(),
        commits: Vec::new(),
        branches: Vec::new(),
        current_branch: String::new(),
        direction: "TB".to_string(),
        title: None,
        acc_title: None,
        acc_descr: None,
        warning_facts: Vec::new(),
    };
    let mut joined_branches = base.clone();
    joined_branches.branches = vec![GitGraphBranchRenderModel {
        name: "main, feature".to_string(),
    }];
    let mut split_branches = base.clone();
    split_branches.branches = vec![
        GitGraphBranchRenderModel {
            name: "main".to_string(),
        },
        GitGraphBranchRenderModel {
            name: "feature".to_string(),
        },
    ];
    assert_ne!(
        render(RenderSemanticModel::GitGraph(joined_branches)),
        render(RenderSemanticModel::GitGraph(split_branches)),
        "branch-list delimiters must not be forgeable by authored branch names"
    );

    let mut joined_identity = base.clone();
    joined_identity.commits.push(GitGraphCommitRenderModel {
        id: "c0 x".to_string(),
        message: String::new(),
        seq: 0,
        commit_type: 0,
        tags: Vec::new(),
        parents: Vec::new(),
        branch: "main".to_string(),
        custom_type: None,
        custom_id: None,
    });
    let mut split_identity = base;
    split_identity.commits.push(GitGraphCommitRenderModel {
        id: "x".to_string(),
        message: String::new(),
        seq: 0,
        commit_type: 0,
        tags: Vec::new(),
        parents: Vec::new(),
        branch: "main c0".to_string(),
        custom_type: None,
        custom_id: None,
    });
    assert_ne!(
        render(RenderSemanticModel::GitGraph(joined_identity)),
        render(RenderSemanticModel::GitGraph(split_identity)),
        "commit branch and id ownership must remain distinguishable"
    );
}
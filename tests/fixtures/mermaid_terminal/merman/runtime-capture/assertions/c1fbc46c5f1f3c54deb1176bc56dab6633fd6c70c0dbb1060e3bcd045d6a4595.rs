fn git_graph_commit_message_and_metadata_are_framed_without_collisions() {
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
    let mut message = base.clone();
    message.commits.push(GitGraphCommitRenderModel {
        id: "c0".to_string(),
        message: "init tags=[v1]".to_string(),
        seq: 0,
        commit_type: 0,
        tags: Vec::new(),
        parents: Vec::new(),
        branch: "main".to_string(),
        custom_type: None,
        custom_id: None,
    });
    let mut metadata = base;
    metadata.commits.push(GitGraphCommitRenderModel {
        id: "c0".to_string(),
        message: "init".to_string(),
        seq: 0,
        commit_type: 0,
        tags: vec!["v1".to_string()],
        parents: Vec::new(),
        branch: "main".to_string(),
        custom_type: None,
        custom_id: None,
    });

    let message_output = render(RenderSemanticModel::GitGraph(message));
    let metadata_output = render(RenderSemanticModel::GitGraph(metadata));
    assert_ne!(message_output, metadata_output);
    assert!(
        message_output.contains("message(bytes=14)=")
            && message_output.contains("init")
            && message_output.contains("tags=[v1]")
    );
    assert!(
        metadata_output.contains("message(bytes=4)=\"init\"")
            && metadata_output.contains("tags=[bytes=2 \"v1\"]")
    );

    let mut comma_message = GitGraphRenderModel {
        diagram_type: "gitGraph".to_string(),
        commits: vec![GitGraphCommitRenderModel {
            id: "c0".to_string(),
            message: "x".to_string(),
            seq: 0,
            commit_type: 0,
            tags: vec!["a, b".to_string()],
            parents: Vec::new(),
            branch: "main".to_string(),
            custom_type: None,
            custom_id: None,
        }],
        branches: Vec::new(),
        current_branch: String::new(),
        direction: "TB".to_string(),
        title: None,
        acc_title: None,
        acc_descr: None,
        warning_facts: Vec::new(),
    };
    let mut comma_tags = comma_message.clone();
    comma_message.commits[0].tags.clear();
    comma_message.commits[0].message = "x tags=[a, b]".to_string();
    comma_tags.commits[0].tags = vec!["a".to_string(), "b".to_string()];
    assert_ne!(
        render(RenderSemanticModel::GitGraph(comma_message)),
        render(RenderSemanticModel::GitGraph(comma_tags.clone())),
        "length-framed fields must distinguish embedded delimiters from list structure"
    );

    let mut leading = comma_tags.clone();
    leading.commits[0].message = " init".to_string();
    leading.commits[0].tags.clear();
    let mut trailing = comma_tags;
    trailing.commits[0].message = "init ".to_string();
    trailing.commits[0].tags.clear();
    assert_ne!(
        render(RenderSemanticModel::GitGraph(leading)),
        render(RenderSemanticModel::GitGraph(trailing)),
        "quoted fields must preserve equal-length leading and trailing whitespace"
    );
}
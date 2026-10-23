#[test]
    fn git_workflow_renders() {
        let output = render_flowchart_fixture("git_workflow.mmd");
        for label in [
            "Working Dir",
            "Staging Area",
            "Local Repo",
            "Remote Repo",
            "git add",
            "git commit",
            "git push",
            "git pull",
        ] {
            assert!(output.contains(label), "Missing '{label}':\n{output}");
        }
    }
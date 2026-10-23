#[test]
    fn git_workflow_lr_forward_and_backward_labels_all_corridor_adjacent() {
        let text = render_flowchart_fixture("git_workflow.mmd");
        for label in ["git add", "git commit", "git push", "git pull"] {
            assert!(
                text.contains(label),
                "git_workflow should include {label:?}. Output:\n{text}"
            );
        }
    }
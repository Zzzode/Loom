#[test]
    fn diamond_text_not_corrupted_by_arrows() {
        let output = render_flowchart_fixture("labeled_edges.mmd");
        assert!(
            output.contains("Valid?"),
            "Diamond text 'Valid?' should be intact in output:\n{output}"
        );
    }
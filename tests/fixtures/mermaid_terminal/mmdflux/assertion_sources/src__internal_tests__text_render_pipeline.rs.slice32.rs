#[test]
    fn inline_label_flowchart_forward_labels_stay_attached_post_phase_3() {
        let text = render_flowchart_fixture("inline_label_flowchart.mmd");
        for label in ["no", "yes", "sync", "async", "hit", "miss", "warn"] {
            assert!(
                text.contains(label),
                "inline_label_flowchart should include {label:?} after Phase 3. Output:\n{text}"
            );
        }
    }
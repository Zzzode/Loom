#[test]
    fn backward_label_asymmetric_markers_retains_approach_turn() {
        let text = render_flowchart_fixture("backward_label_asymmetric_markers.mmd");
        assert!(
            text.contains("┌───┘"),
            "backward_label_asymmetric_markers must retain the `┌───┘` approach-turn glyph. Output:\n{text}"
        );
    }
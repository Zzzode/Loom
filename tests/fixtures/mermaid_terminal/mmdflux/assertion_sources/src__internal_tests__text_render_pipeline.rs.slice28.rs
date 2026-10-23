#[test]
    fn label_clamp_bt_review_retains_corridor_turn_glyphs() {
        let text = render_flowchart_fixture("label_clamp_bt_review.mmd");
        assert!(
            text.contains("└─┐"),
            "label_clamp_bt_review must retain the `└─┐` corridor-turn glyph. Output:\n{text}"
        );
        assert!(
            text.contains("┌─┘"),
            "label_clamp_bt_review must retain the `┌─┘` corridor-turn glyph. Output:\n{text}"
        );
    }
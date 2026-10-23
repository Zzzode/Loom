#[test]
    fn state_cjk_labels_box_borders_align_in_terminal_columns() {
        let text = render_state_fixture("cjk_labels.mmd");
        assert_box_columns_aligned("state/cjk_labels", &text);
    }
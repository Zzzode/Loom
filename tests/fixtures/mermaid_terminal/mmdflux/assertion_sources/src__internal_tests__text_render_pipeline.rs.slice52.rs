#[test]
    fn flowchart_cjk_labels_box_borders_align_in_terminal_columns() {
        let text = render_flowchart_fixture("cjk_labels.mmd");
        assert_box_columns_aligned("flowchart/cjk_labels", &text);
    }
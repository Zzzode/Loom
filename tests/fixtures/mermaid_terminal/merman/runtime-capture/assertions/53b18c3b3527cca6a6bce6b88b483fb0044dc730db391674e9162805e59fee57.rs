fn tree_view_render_model_renders_outline_summary() {
    let model = TreeViewDiagramRenderModel {
        acc_title: Some("Tree title".to_string()),
        acc_descr: Some("Tree description".to_string()),
        title: Some("Project".to_string()),
        root: tree_node(
            0,
            -1,
            "/",
            vec![
                tree_node(
                    1,
                    0,
                    "Root",
                    vec![
                        tree_node(2, 1, "Child 1", Vec::new()),
                        tree_node(3, 1, "Child 2", Vec::new()),
                    ],
                ),
                tree_node(4, 0, "Sibling", Vec::new()),
            ],
        ),
    };

    let rendered = render(RenderSemanticModel::TreeView(model));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=7)=\"Project\"\n",
            "accTitle(bytes=10)=\"Tree title\"\n",
            "accDescr(bytes=16)=\"Tree description\"\n",
            "[directory] name(bytes=1)=\"/\" [id=0, level=-1]\n",
            "|-- [directory] name(bytes=4)=\"Root\"/ [id=1, level=0]\n",
            "|   |-- [file] name(bytes=7)=\"Child 1\" [id=2, level=1]\n",
            "|   \\-- [file] name(bytes=7)=\"Child 2\" [id=3, level=1]\n",
            "\\-- [file] name(bytes=7)=\"Sibling\" [id=4, level=0]",
        )
    );
}
fn tree_view_discloses_typed_fields_and_honors_unicode_connectors() {
    let file = TreeViewNodeRenderModel {
        id: 2,
        level: 1,
        name: "App.tsx".to_string(),
        node_type: "file".to_string(),
        icon: Some("react".to_string()),
        description: Some("main component".to_string()),
        ..Default::default()
    };
    let directory = TreeViewNodeRenderModel {
        id: 1,
        level: 0,
        name: "src".to_string(),
        node_type: "directory".to_string(),
        css_class: Some("highlight".to_string()),
        icon: Some("folder".to_string()),
        description: Some("source directory".to_string()),
        children: vec![file],
    };
    let model = TreeViewDiagramRenderModel {
        root: TreeViewNodeRenderModel {
            children: vec![directory],
            ..Default::default()
        },
        ..Default::default()
    };

    let ascii = render_with_options(
        RenderSemanticModel::TreeView(model.clone()),
        &AsciiRenderOptions::ascii(),
    );
    let unicode = render_with_options(
        RenderSemanticModel::TreeView(model),
        &AsciiRenderOptions::unicode(),
    );

    assert_eq!(
        ascii,
        concat!(
            "[directory] name(bytes=1)=\"/\" [id=0, level=-1]\n",
            "\\-- [directory] name(bytes=3)=\"src\"/ [id=1, level=0, icon(bytes=6)=\"folder\",\n",
            "    class(bytes=9)=\"highlight\"] description(bytes=16)=\"source directory\"\n",
            "    \\-- [file] name(bytes=7)=\"App.tsx\" [id=2, level=1, icon(bytes=5)=\"react\"]\n",
            "        description(bytes=14)=\"main component\"",
        )
    );
    assert_eq!(
        unicode,
        concat!(
            "[directory] name(bytes=1)=\"/\" [id=0, level=-1]\n",
            "└── [directory] name(bytes=3)=\"src\"/ [id=1, level=0, icon(bytes=6)=\"folder\",\n",
            "    class(bytes=9)=\"highlight\"] description(bytes=16)=\"source directory\"\n",
            "    └── [file] name(bytes=7)=\"App.tsx\" [id=2, level=1, icon(bytes=5)=\"react\"]\n",
            "        description(bytes=14)=\"main component\"",
        )
    );
}
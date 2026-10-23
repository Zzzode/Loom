fn tree_view_distinguishes_trailing_slash_files_from_directories() {
    let file = TreeViewNodeRenderModel {
        id: 1,
        level: 0,
        name: "src/".to_string(),
        node_type: "file".to_string(),
        ..Default::default()
    };
    let directory = TreeViewNodeRenderModel {
        node_type: "directory".to_string(),
        ..file.clone()
    };

    let file_model = TreeViewDiagramRenderModel {
        root: file,
        ..Default::default()
    };
    let directory_model = TreeViewDiagramRenderModel {
        root: directory,
        ..Default::default()
    };

    let file_rendered = render(RenderSemanticModel::TreeView(file_model));
    let directory_rendered = render(RenderSemanticModel::TreeView(directory_model));

    assert!(file_rendered.starts_with("[file] name(bytes=4)=\"src/\" "));
    assert!(directory_rendered.starts_with("[directory] name(bytes=4)=\"src/\" "));
    assert_ne!(file_rendered, directory_rendered);
}
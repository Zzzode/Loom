fn flowchart_duplicate_subgraph_vertex_css_updates_the_canonical_group() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TB\n",
            "classDef hot stroke:#123456\n",
            "subgraph X[First]\nA\nend\n",
            "subgraph X[Second]\nB\nend\n",
            "style X fill:#010203\n",
            "class X hot\n",
        ),
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html),
    )
    .expect("duplicate subgraph declarations should render");

    assert!(rendered.contains("background-color:#010203"), "{rendered}");
    assert!(rendered.contains("color:#123456"), "{rendered}");
}
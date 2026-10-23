#[test]
    fn render_payload_annotates_node_ids_when_show_ids_is_set() {
        let config = RenderConfig {
            show_ids: true,
            ..RenderConfig::default()
        };
        let output = crate::runtime::render_diagram(
            "graph TD\nA[Start] --> B[End]",
            OutputFormat::Text,
            &config,
        )
        .unwrap();
        assert!(
            output.contains("A: Start"),
            "output should contain annotated ID: {output}"
        );
        assert!(
            output.contains("B: End"),
            "output should contain annotated ID: {output}"
        );
    }
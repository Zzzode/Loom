#[test]
    fn render_payload_skips_annotation_when_show_ids_is_false() {
        let config = RenderConfig::default();
        let output = crate::runtime::render_diagram(
            "graph TD\nA[Start] --> B[End]",
            OutputFormat::Text,
            &config,
        )
        .unwrap();
        assert!(
            !output.contains("A:"),
            "output should not have ID prefix: {output}"
        );
    }
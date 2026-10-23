#[test]
    fn render_diagram_emits_render_trace_context() {
        let input = "graph TD\nA-->B";
        let collected = collect_trace_events_with_scoped_subscriber(|| {
            let _ = render_diagram(input, OutputFormat::Text, &RenderConfig::default())
                .expect("render");
        });

        assert!(collected.contains_target("mmdflux::runtime"));
        assert!(collected.contains_field("format", "text"));
        assert!(collected.contains_field("input_bytes", &input.len().to_string()));
        assert!(collected.contains_field("frontend", "mermaid"));
        assert!(collected.contains_field("diagram_type", "flowchart"));
        assert!(collected.contains_field("geometry_level", "layout"));
    }
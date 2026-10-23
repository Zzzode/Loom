#[test]
    fn http_request_renders() {
        let output = render_flowchart_fixture("http_request.mmd");
        assert!(!output.is_empty());
        let has_nodes = output.contains("Client")
            || output.contains("Server")
            || output.contains("Process")
            || output.contains("Response");
        assert!(has_nodes, "Should contain at least one node label");
        assert!(
            output.contains('<') || output.contains('>'),
            "Should have decision node (diamond shape uses < or > chars)"
        );
    }
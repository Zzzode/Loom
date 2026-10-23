fn schema_three_serializes_requested_and_selected_layout_without_conflating_fallback() {
    let model = parse_model(LONG_NODE_CHAIN);
    let output = render_model_report(
        &model,
        &AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Auto),
        AsciiViewportPolicy::with_max_width(120),
    )
    .expect("automatic fitting should produce a report");
    let metadata = serde_json::to_value(output.metadata()).expect("metadata should serialize");
    assert_eq!(metadata["schema_version"], 3);
    assert_eq!(metadata["requested_layout_profile"], "auto");
    assert_eq!(metadata["layout_profile"], "compact");
    assert_eq!(metadata["compact_attempted"], true);
    assert_eq!(metadata["fallback_attempted"], false);
    let report = serde_json::to_value(output.report()).expect("report should serialize");
    for (field, value) in metadata.as_object().expect("metadata object") {
        assert_eq!(
            &report[field], value,
            "report and metadata differ at {field}"
        );
    }
    assert_eq!(report["text"], output.text);
}
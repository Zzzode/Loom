fn canonical_report_payload_contains_the_binding_metadata_subset() {
    let report = render_report(
        "timeline\ntitle Basic\n2024 : Event",
        AsciiViewportPolicy::default(),
    )
    .expect("report should render");
    let json = serde_json::to_value(report.report()).expect("report should serialize");
    let metadata = report.metadata();

    assert_eq!(json["kind"], "ascii");
    assert_eq!(json["schema_version"], metadata.schema_version);
    assert_eq!(json["family"], metadata.family);
    assert_eq!(json["projection"], metadata.projection);
    assert_eq!(json["encoding"], metadata.encoding);
    assert_eq!(json["primary_width"], metadata.primary_width);
    assert_eq!(json["emitted_height"], metadata.emitted_height);
    assert_eq!(json["text"], report.as_text());
}
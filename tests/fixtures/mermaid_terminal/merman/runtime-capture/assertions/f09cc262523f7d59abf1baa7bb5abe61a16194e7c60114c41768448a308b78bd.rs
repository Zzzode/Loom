fn xychart_empty_projection_obeys_exact_output_byte_budget() {
    let input = concat!(
        "xychart\n",
        "title 空\n",
        "x-axis [alpha, beta]\n",
        "y-axis Score 0 --> 10\n",
    );
    let rendered = render_xychart(input, &AsciiRenderOptions::ascii())
        .expect("empty XYChart should render under the default budget");
    let exact = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxOutputBytes, rendered.len())
        .expect("valid exact output-byte limit");

    assert_eq!(
        render_xychart_with_resources(input, &AsciiRenderOptions::ascii(), exact)
            .expect("exact empty-chart byte budget should render"),
        rendered
    );

    let below = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxOutputBytes, rendered.len() - 1)
        .expect("valid N-1 output-byte limit");
    let error = render_xychart_with_resources(input, &AsciiRenderOptions::ascii(), below)
        .expect_err("N-1 empty-chart byte budget should reject the final document");
    let AsciiError::ResourceLimitExceeded(details) = error else {
        panic!("expected output-byte resource error, got {error:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxOutputBytes);
    assert_eq!(details.actual, rendered.len());
    assert_eq!(details.max, rendered.len() - 1);
}
fn xychart_empty_projection_obeys_exact_document_cell_budget() {
    let input = concat!(
        "xychart horizontal\n",
        "title Lost\n",
        "x-axis [alpha, beta]\n",
        "y-axis Score 0 --> 10\n",
    );
    let rendered = render_xychart(input, &AsciiRenderOptions::ascii())
        .expect("empty XYChart should render under the default budget");
    let exact_cells = rendered.lines().map(str::len).sum::<usize>();
    let exact = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxDocumentCells, exact_cells)
        .expect("valid exact document-cell limit");

    assert_eq!(
        render_xychart_with_resources(input, &AsciiRenderOptions::ascii(), exact)
            .expect("exact empty-chart cell budget should render"),
        rendered
    );

    let below = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxDocumentCells, exact_cells - 1)
        .expect("valid N-1 document-cell limit");
    let error = render_xychart_with_resources(input, &AsciiRenderOptions::ascii(), below)
        .expect_err("N-1 empty-chart cell budget should reject the document");
    let AsciiError::ResourceLimitExceeded(details) = error else {
        panic!("expected document-cell resource error, got {error:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxDocumentCells);
    assert_eq!(details.actual, exact_cells);
    assert_eq!(details.max, exact_cells - 1);
}
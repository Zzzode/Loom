fn flowchart_parser_manual_input_and_stored_data_aliases_keep_distinct_geometry() {
    let manual_inputs = ["sl-rect", "manual-input", "sloped-rectangle"].map(|shape| {
        render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Input\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap()
    });
    assert!(manual_inputs.windows(2).all(|pair| pair[0] == pair[1]));
    assert!(manual_inputs[0].contains("Input"));
    assert!(
        manual_inputs[0].lines().any(|line| line.starts_with('/')),
        "manual-input aliases should retain the sloped input edge:\n{}",
        manual_inputs[0]
    );

    let stored_data = ["bow-rect", "stored-data", "bow-tie-rectangle"].map(|shape| {
        render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Data\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap()
    });
    assert!(stored_data.windows(2).all(|pair| pair[0] == pair[1]));
    assert!(stored_data[0].contains("Data"));
    assert!(
        stored_data[0].contains(')') && stored_data[0].contains('('),
        "stored-data aliases should retain concave bow-tie sides:\n{}",
        stored_data[0]
    );
}
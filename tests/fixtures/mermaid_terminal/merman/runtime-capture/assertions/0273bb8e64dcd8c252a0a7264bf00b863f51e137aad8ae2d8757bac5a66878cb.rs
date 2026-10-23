fn packet_render_model_rejects_noninclusive_bit_counts() {
    let mut model = PacketDiagramRenderModel::default();
    model.packet = vec![vec![PacketRenderBlock {
        start: 11,
        end: 31,
        bits: 20,
        label: "multiple".to_string(),
    }]];

    let error = render_model(
        &RenderSemanticModel::Packet(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("packet range width must be validated before rendering");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "packet",
            feature: "packet block bit count does not match inclusive range",
        }
    );
}
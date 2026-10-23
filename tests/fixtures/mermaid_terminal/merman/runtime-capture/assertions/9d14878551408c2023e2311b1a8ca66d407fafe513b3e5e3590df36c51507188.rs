fn packet_labels_cannot_forge_following_block_boundaries() {
    let mut forged = PacketDiagramRenderModel::default();
    forged.packet = vec![vec![PacketRenderBlock {
        start: 0,
        end: 7,
        bits: 8,
        label: "header (8 bits) | [8..15] payload".to_string(),
    }]];
    let mut split = PacketDiagramRenderModel::default();
    split.packet = vec![vec![
        PacketRenderBlock {
            start: 0,
            end: 7,
            bits: 8,
            label: "header".to_string(),
        },
        PacketRenderBlock {
            start: 8,
            end: 15,
            bits: 8,
            label: "payload".to_string(),
        },
    ]];

    assert_ne!(
        render(RenderSemanticModel::Packet(forged)),
        render(RenderSemanticModel::Packet(split)),
        "length-framed packet labels must distinguish one authored block from two blocks"
    );

    let mut leading = PacketDiagramRenderModel::default();
    leading.packet = vec![vec![PacketRenderBlock {
        start: 0,
        end: 7,
        bits: 8,
        label: " label".to_string(),
    }]];
    let mut trailing = PacketDiagramRenderModel::default();
    trailing.packet = vec![vec![PacketRenderBlock {
        start: 0,
        end: 7,
        bits: 8,
        label: "label ".to_string(),
    }]];
    assert_ne!(
        render(RenderSemanticModel::Packet(leading)),
        render(RenderSemanticModel::Packet(trailing)),
        "equal-length whitespace variants must remain distinguishable after wrapping"
    );
}
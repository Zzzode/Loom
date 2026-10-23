fn packet_render_model_renders_rows_and_ranges() {
    let mut model = PacketDiagramRenderModel::default();
    model.title = Some("Packet".to_string());
    model.acc_title = Some("Packet title".to_string());
    model.acc_descr = Some("Packet description".to_string());
    model.packet = vec![
        vec![
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
        ],
        vec![PacketRenderBlock {
            start: 16,
            end: 31,
            bits: 16,
            label: "footer".to_string(),
        }],
    ];

    let rendered = render(RenderSemanticModel::Packet(model));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=6)=\"Packet\"\n",
            "accTitle(bytes=12)=\"Packet title\"\n",
            "accDescr(bytes=18)=\"Packet description\"\n",
            "row 1:\n",
            "  - range=[0..7] bits=8 label(bytes=6)=\"header\"\n",
            "  - range=[8..15] bits=8 label(bytes=7)=\"payload\"\n",
            "row 2:\n",
            "  - range=[16..31] bits=16 label(bytes=6)=\"footer\"",
        )
    );
}
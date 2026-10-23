fn packet_parser_split_blocks_render_inclusive_bit_counts() {
    let rendered = render_parsed(
        r#"packet
0-10: "test"
11-90: "multiple"
"#,
    );

    assert_eq!(
        rendered,
        concat!(
            "row 1:\n",
            "  - range=[0..10] bits=11 label(bytes=4)=\"test\"\n",
            "  - range=[11..31] bits=21 label(bytes=8)=\"multiple\"\n",
            "row 2:\n",
            "  - range=[32..63] bits=32 label(bytes=8)=\"multiple\"\n",
            "row 3:\n",
            "  - range=[64..90] bits=27 label(bytes=8)=\"multiple\"",
        )
    );
}
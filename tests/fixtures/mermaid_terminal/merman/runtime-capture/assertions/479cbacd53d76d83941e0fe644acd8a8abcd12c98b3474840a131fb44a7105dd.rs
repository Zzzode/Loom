fn er_color_html_wraps_dense_relation_summary_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::NodeBorder, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x202020))
        .with_role(AsciiColorRole::Section, AsciiRgb::from_hex24(0x303030))
        .with_role(AsciiColorRole::EdgeLabel, AsciiRgb::from_hex24(0x505050));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::Html)
        .with_color_theme(theme);

    let rendered = render_er(
        "erDiagram\nA ||--|| B : ab\nB ||--|| A : ba\nA ||--|| C : ac\nC ||--|| A : ca\nB ||--|| C : bc\nC ||--|| B : cb",
        &options,
    )
    .expect("dense ER diagram should render");

    let ab = framed_er_summary_relation("A", "||--||", "B", "ab");
    let ba = framed_er_summary_relation("B", "||--||", "A", "ba");
    let ac = framed_er_summary_relation("A", "||--||", "C", "ac");
    let ca = framed_er_summary_relation("C", "||--||", "A", "ca");
    let bc = framed_er_summary_relation("B", "||--||", "C", "bc");
    let cb = framed_er_summary_relation("C", "||--||", "B", "cb");
    let html_ab = ab.replace('"', "&quot;");
    let html_ba = ba.replace('"', "&quot;");
    let html_ac = ac.replace('"', "&quot;");
    let html_ca = ca.replace('"', "&quot;");
    let html_bc = bc.replace('"', "&quot;");
    let html_cb = cb.replace('"', "&quot;");
    assert_eq!(
        strip_html_spans(&rendered),
        format!(
            "+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\n+---+\n| C |\n+---+\n\nrelations:\n{html_ab}\n{html_ba}\n{html_ac}\n{html_ca}\n{html_bc}\n{html_cb}\n"
        )
    );
    for expected_fragment in [
        "<span style=\"color:#101010\">+---+</span>".to_string(),
        "<span style=\"color:#202020\">A</span>".to_string(),
        "<span style=\"color:#303030\">relations:</span>".to_string(),
        format!("<span style=\"color:#505050\">{html_ab}</span>"),
    ] {
        assert!(
            rendered.contains(&expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}
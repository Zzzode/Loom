#[test]
fn render_surfaces_include_text_svg_layout_and_routed_mmds() {
    let pair = mutations::pair_by_id("M06").expect("M06 pair should exist");
    let rendered = render_surfaces::render_pair(pair).expect("pair should render");

    assert!(!rendered.before.text.is_empty());
    assert!(!rendered.after.text.is_empty());
    assert!(rendered.before.svg.contains("<svg"));
    assert!(rendered.after.svg.contains("<svg"));
    assert_eq!(
        rendered.before.layout_mmds.geometry_level,
        GeometryLevel::Layout
    );
    assert_eq!(
        rendered.before.routed_mmds.geometry_level,
        GeometryLevel::Routed
    );
    assert_eq!(
        rendered.after.layout_mmds.geometry_level,
        GeometryLevel::Layout
    );
    assert_eq!(
        rendered.after.routed_mmds.geometry_level,
        GeometryLevel::Routed
    );
}
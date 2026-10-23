#[test]
fn render_surfaces_allow_lossless_routed_mmds_for_style_canaries() {
    let pair = mutations::pair_by_id("S03").expect("S03 pair should exist");
    let rendered = render_surfaces::render_pair(pair).expect("pair should render");
    let (_, lossless) =
        render_surfaces::render_lossless_routed_mmds(&rendered.after.source).unwrap();

    assert_eq!(
        rendered.after.routed_mmds.geometry_level,
        GeometryLevel::Routed
    );
    assert_eq!(lossless.geometry_level, GeometryLevel::Routed);
}
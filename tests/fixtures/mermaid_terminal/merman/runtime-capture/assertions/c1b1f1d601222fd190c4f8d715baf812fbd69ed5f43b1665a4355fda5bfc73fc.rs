fn class_parser_member_classifiers_remain_visible() {
    let rendered = render_class(
        "classDiagram\nclass Service {\n  +abstractValue*\n  +staticValue$\n  +abstractCall()*\n  +staticCall()$\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("classified class members should render");

    for expected in [
        "+abstractValue*",
        "+staticValue$",
        "+abstractCall()*",
        "+staticCall()$",
    ] {
        assert!(
            rendered.contains(expected),
            "classified member {expected:?} should remain visible:\n{rendered}"
        );
    }
}
fn xychart_parser_empty_chart_preserves_title_and_axis_semantics() {
    let rendered = render_xychart(
        concat!(
            "xychart horizontal\n",
            "title Lost\n",
            "x-axis [alpha, beta]\n",
            "y-axis Score 0 --> 10\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("empty authored xychart should render");

    assert_eq!(
        rendered,
        concat!(
            "xychart: empty\n",
            "orientation: horizontal\n",
            "title(bytes=4)=\"Lost\"\n",
            "xAxis: band title(bytes=0)=\"\" categories=[bytes=5 \"alpha\", bytes=4 \"beta\"]\n",
            "yAxis: linear title(bytes=5)=\"Score\" min=0 max=10\n",
            "display: showTitle=true showDataLabel=false showDataLabelOutsideBar=false xAxis={showLabel=true showTitle=true showTick=true showAxisLine=true} yAxis={showLabel=true showTitle=true showTick=true showAxisLine=true}\n",
            "plots: []",
        )
    );
}
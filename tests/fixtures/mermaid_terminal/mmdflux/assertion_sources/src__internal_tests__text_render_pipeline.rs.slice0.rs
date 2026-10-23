#[test]
    fn ascii_only_mode() {
        let unicode_output = render_flowchart_fixture("simple.mmd");
        let ascii_output = render_flowchart_fixture_ascii("simple.mmd");

        assert!(unicode_output.contains("Start"));
        assert!(ascii_output.contains("Start"));

        let unicode_chars = [
            '─', '│', '┌', '┐', '└', '┘', '├', '┤', '┬', '┴', '┼', '╭', '╮', '╯', '╰',
        ];
        for ch in unicode_chars {
            assert!(
                !ascii_output.contains(ch),
                "ASCII output should not contain '{ch}'"
            );
        }
    }
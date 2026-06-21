# Test: Pre and Code Blocks

Source files for [`test/epubs/test_pre_code_blocks.epub`](../../epubs/test_pre_code_blocks.epub).

This fixture focuses on EPUB whitespace behavior used by `<pre>`, `<code>`, and CSS `white-space` rules:

1. Inline `<code>` in normal paragraph flow.
2. `<pre>` blocks with indentation, tabs, blank lines, and escaped entities.
3. CSS `white-space: normal`, `pre-wrap`, and `pre-line`.
4. Long code-like tokens that should wrap inside the viewport without overlap.

Build from this directory:

```sh
rm -f ../../epubs/test_pre_code_blocks.epub
zip -X0 ../../epubs/test_pre_code_blocks.epub mimetype
zip -Xr9D ../../epubs/test_pre_code_blocks.epub META-INF OEBPS
```

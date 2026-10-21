# XML Validator and Binary Heap

Two small C tools in one CLI:

- `validate FILE` checks tag nesting with a dynamically sized string stack.
  It handles XML declarations, comments, CDATA, quoted attributes, and
  self-closing tags, and reports an approximate byte offset on failure.
- `heap FILE` reads signed integers, constructs a max heap, and writes a
  nested XML tree with node IDs and values to standard output.

This validator demonstrates stack-based parsing. It is not a complete XML
standards implementation: DTDs and entity expansion are intentionally
unsupported. Use a standards-compliant XML library for untrusted production
documents.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic src/main.c -o xmlheap
python tests/test_cli.py
```

Examples:

```sh
./xmlheap validate examples/nested.xml
./xmlheap heap examples/numbers.txt > tree.xml
./xmlheap validate tree.xml
```


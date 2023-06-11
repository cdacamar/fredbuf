# Project

This is the text buffer implementation described in [Text Editor Data Structures](https://cdacamar.github.io/data%20structures/algorithms/benchmarking/text%20editors/c++/editor-data-structures/?fbclid=IwAR1KPqHQU-torrzSq7LKWgK3uUZsTaoEpiAQeDT8XlvlOD3MCSt3sEl2YXc).

## API

Building:

```c++
using namespace PieceTree;
TreeBuilder builder;
builder.accept("ABC");
builder.accept("DEF");
auto tree = builder.create();
// Resulting total buffer: "ABCDEF"
```

Insertion:

```c++
tree.insert(CharOffset{ 0 }, "foo");
// Resulting total buffer: "fooABCDEF"
```

Deletion:

```c++
tree.remove(CharOffset{ 6 }, Length{ 3 });
// Resulting total buffer: "fooABC"
```

Line retrieval:

```c++
std::string buf;
tree.get_line_content(&buf, Line{ 1 });
// 'buf' contains "fooABC"
```

Iteration:

```c++
for (char c : buf)
{
    printf("%c", c);
}
```

## Contributing

Feel free to open up a PR or issue but there's no guarantee it will get merged.

## Building

If you're on Windows, open a developer command prompt and invoke `b.bat`.  Do the same for essentially any other compiler except change the flags to be specific to your compiler.  It's all standard C++ all the way down.
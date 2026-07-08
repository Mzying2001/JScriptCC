# JScriptCC

A C++11 library that preprocesses IE JScript Conditional Compilation (`@cc_on`) syntax into plain JavaScript, designed for WebBrowser/WebBrowser2 hook scenarios where modern JS engines need to handle legacy IE-specific code.

## Features

- Converts `@cc_on`, `@if`, `@elif`, `@else`, `@end`, `@set` directives to plain JS
- No JS parser — modern JS (ES2025+) passes through untouched
- Correctly ignores `@` directives inside strings, comments, regex, and template literals
- Supports `/*@cc_on ... @*/` block form and `//@cc_on` line form
- Also supports `/*@if ... @*/` without `cc_on` (matches old IE behavior)
- Full expression evaluation with arithmetic, comparison, logical, and bitwise operators
- Predefined variables: `@_jscript`, `@_jscript_version`, `@_win32`, `@_win64`, `@_x86`, `@_amd64`, `@_debug`, etc.
- User-definable variables via `@set` and `CCEnvironment`
- Error recovery — continues parsing on malformed input
- C++11, zero dependencies, RAII, `std::unique_ptr` for AST memory

## Quick Start

```cpp
#include "jscriptcc/CCPreprocessor.h"
#include <iostream>

int main() {
    // Both forms are equivalent:
    std::string source = R"(
        foo();
        /*@cc_on
        @if(@_win32)
        alert('Windows');
        @else
        alert('Other');
        @end
        @*/
        bar();
    )";

    // Or without cc_on (matching old IE behavior):
    // /*@if(@_win32)
    // alert('Windows');
    // @else
    // alert('Other');
    // @end
    // @*/

    jscriptcc::CCPreprocessor pp;
    std::string output;
    pp.Process(source, output);

    std::cout << output;
    // foo();
    // alert('Windows');
    // bar();
}
```

## API

### CCPreprocessor

```cpp
class CCPreprocessor {
public:
    bool Process(
        const std::string& source,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        std::vector<CCError>* errors = nullptr);
};
```

- `source` — UTF-8 JavaScript with possible `@cc_on` blocks
- `output` — UTF-8 plain JavaScript with CC expanded
- `env` — environment with predefined and user variables
- `errors` — optional, receives error list with line/column info
- Returns `true` if no fatal errors

### CCEnvironment

```cpp
jscriptcc::CCEnvironment env;
env.set("@_win32", jscriptcc::CCValue(0.0));       // override predefined
env.set("@MY_FLAG", jscriptcc::CCValue(1.0));      // add custom variable
env.set("@VERSION", jscriptcc::CCValue("2.0"));    // string value
```

Default predefined variables:

| Variable           | Default               | Description          |
| ------------------ | --------------------- | -------------------- |
| `@_jscript`        | 1                     | Always 1 in JScript  |
| `@_jscript_version`| 5.8                   | JScript version      |
| `@_win16`          | 0                     | 16-bit Windows       |
| `@_win32`          | `sizeof(void*) != 8`  | 32-bit Windows       |
| `@_win64`          | `sizeof(void*) == 8`  | 64-bit Windows       |
| `@_x86`            | `sizeof(void*) != 8`  | x86 processor        |
| `@_amd64`          | `sizeof(void*) == 8`  | AMD64 processor      |
| `@_mac`            | 0                     | macOS                |
| `@_debug`          | 0                     | Debug mode           |

> `@_win32`/`@_win64` and `@_x86`/`@_amd64` are set at compile time based on the program's pointer size. A 64-bit build gets `@_win64=1, @_amd64=1`; a 32-bit build gets `@_win32=1, @_x86=1`.

### CCError

```cpp
struct CCError {
    int line;
    int column;
    std::string message;
};
```

## Supported Syntax

```text
/*@cc_on                    — activate CC (block form, until @*/)
//@cc_on                    — activate CC (line form, rest of source)
/*@if (condition)           — CC block without cc_on (matches old IE behavior)
@if (condition)             — conditional branch
@elif (condition)           — else-if branch
@else                       — else branch
@end                        — end conditional block
@set @var = expression      — assign CC variable
```

Expressions support: `!`, `~`, `+`, `-`, `*`, `/`, `%`, `<<`, `>>`, `<`, `<=`, `>`, `>=`, `==`, `!=`, `&`, `^`, `|`, `&&`, `||`, parentheses.

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

Requires C++11. No third-party dependencies.

### Run tests

```bash
cd build
ctest
# or
./jscriptcc_tests
```

## Integration

Add to your CMake project:

```cmake
add_subdirectory(JScriptCC)
target_link_libraries(your_target jscriptcc)
```

Or compile directly — only the `src/*.cpp` files and `include/` directory are needed.

## License

MIT

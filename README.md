# JScriptCC

A C++11 library that preprocesses IE JScript Conditional Compilation (`@cc_on`) syntax into plain JavaScript, designed for WebBrowser/WebBrowser2 hook scenarios where modern JS engines need to handle legacy IE-specific code.

## Features

- Converts JScript conditional compilation directives into plain JavaScript
- Supports block and line forms, including `/*@if ... @*/` without `cc_on`
- Preserves modern JavaScript, including strings, comments, regex, and template literals
- Evaluates arithmetic, comparison, logical, and bitwise expressions
- Provides IE-compatible predefined variables and custom variables via `@set` or `CCEnvironment`
- Offers error recovery in a dependency-free C++11 library

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
    pp.process(source, output);

    std::cout << output;
    // foo();
    // alert('Windows');
    // bar();
}
```

## API

The stable public API consists of `CCPreprocessor.h`, `CCEnvironment.h`, `CCValue.h`, and `CCError.h`. Scanner, tokenizer, parser, AST, evaluator, generator, and string-slice types are implementation details and are not public extension points.

### CCPreprocessor

```cpp
class CCPreprocessor {
public:
    bool process(
        const std::string& source,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);

    bool process(
        const char* data,
        std::size_t size,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);

    bool process(
        const std::wstring& source,
        std::wstring& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);

    bool process(
        const wchar_t* data,
        std::size_t size,
        std::wstring& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);
};
```

- Narrow strings contain UTF-8 JavaScript; wide strings use the platform's UTF-16 or UTF-32 `wchar_t` representation
- Pointer overload sizes are code-unit counts and may include embedded null characters
- Wide input is validated strictly; invalid surrogate sequences or Unicode code points return `false` without changing the source text copied to `output`
- Error columns from wide overloads count `wchar_t` code units; existing errors supplied by the caller are left unchanged
- `env` — environment with predefined and user variables
- `errors` — optional, receives error messages as UTF-8 `std::string` values with line/column info
- Returns `true` if no fatal errors

Wide strings are converted at the API boundary and use the same scanner, parser, evaluator, and `CCEnvironment` as UTF-8 input. Environment variable names and string values therefore continue to use the existing UTF-8 `std::string` API.

### CCEnvironment

```cpp
jscriptcc::CCEnvironment env;
env.set("@_win32", jscriptcc::CCValue(0.0));       // override predefined
env.set("@MY_FLAG", jscriptcc::CCValue(1.0));      // add custom variable
env.set("@VERSION", jscriptcc::CCValue("2.0"));    // string value

jscriptcc::CCEnvironment win64(jscriptcc::TargetArchitecture::Win64);
```

Default predefined variables:

| Variable            | Default | Description         |
| ------------------- | ------- | ------------------- |
| `@_jscript`         | 1       | Always 1 in JScript |
| `@_jscript_version` | 5.8     | JScript version     |
| `@_win16`           | 0       | 16-bit Windows      |
| `@_win32`           | 1       | 32-bit Windows      |
| `@_win64`           | 0       | 64-bit Windows      |
| `@_x86`             | 1       | x86 processor       |
| `@_amd64`           | 0       | AMD64 processor     |
| `@_mac`             | 0       | macOS               |
| `@_debug`           | 0       | Debug mode          |

> The default environment targets Win32 JScript 5.8 on every host. Pass `TargetArchitecture::Win64` to the `CCEnvironment` constructor to select the Win64/AMD64 predefined variables explicitly.

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

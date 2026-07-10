# AGENTS.md

This file provides guidance to AI coding agents when working with code in this repository.

## Project Overview

JScriptCC is a C++11 library that preprocesses IE JScript Conditional Compilation (`@cc_on`) syntax into plain JavaScript, for use in modern JS engines (Chromium/WebView2).

Converts this:

```js
foo();
/*@cc_on
@if(@_win32)
alert(1);
@else
alert(2);
@end
@*/
bar();
```

Or equivalently (cc_on omitted, matching old IE behavior):

```js
foo();
/*@if(@_win32)
alert(1);
@else
alert(2);
@end
@*/
bar();
```

Into this:

```js
foo();
alert(1);
bar();
```

Normal JavaScript (including ES2025+) passes through unchanged. Strings, comments, regex literals, and template literals containing `@` directives are preserved as-is.

## Project Structure

```
include/jscriptcc/
    CCPreprocessor.h    # Main preprocessing API
    CCEnvironment.h     # Predefined variables (@_win32, @_jscript, etc.)
    CCValue.h           # Variant type (number/string/bool)
    CCError.h           # Error struct with line/column

src/
    detail/             # Internal scanner/tokenizer/parser/AST/evaluator headers
    (library implementation files)

tests/
    TestFramework.*     # Self-contained test registration and runner support
    test_*.cpp          # Tests grouped by responsibility
```

## Architecture

```
Source string
    → Scanner (state machine, tracks strings/comments/regex/templates)
    → segments: [NormalJS | CCBlock | CCOnLine]
    → Generator:
        NormalJS → copy verbatim
        CCBlock  → Tokenizer → Parser → Evaluator → selected branch text
    → Output string
```

## Build

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"   # or any generator
cmake --build .
./jscriptcc_tests.exe
```

Requires C++11. No third-party dependencies.

## Key Rules

- **No JS parser.** The Scanner only recognizes CC directives, strings, comments, regex, and template literals. Everything else is opaque text.
- **`//@cc_on`** activates CC for the rest of the source (after the line).
- **`/*@cc_on ... @*/`** is a self-contained CC block.
- **`/*@if ... @*/`** (without `cc_on`) is also valid — matches old IE behavior where `cc_on` can be omitted in the block form. Same applies to `/*@elif`, `/*@else`, `/*@end`, `/*@set`.
- **`// @cc_on`** (with space) is a regular comment, NOT a CC trigger.
- **`/* @cc_on */`** (with space) is a regular block comment, NOT a CC trigger.
- Expression parser uses Pratt parsing with correct operator precedence.
- `@set` creates CC-scoped variables; `@if`/`@elif`/`@else`/`@end` control output.

## Git Commit Conventions

- Use Conventional Commits: `<type>(<scope>): <summary>`
- Example types: `feat`, `fix`, `refactor`, `build`, `docs`, `test`, `chore`
- Use an optional scope for the affected project, area, class, or file
- Write concise English summaries
- For non-trivial changes, include a body explaining the reason and implementation/fix approach

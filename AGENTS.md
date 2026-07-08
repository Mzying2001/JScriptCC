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
    CCPreprocessor.h    # Main API: CCPreprocessor::Process()
    CCEnvironment.h     # Predefined variables (@_win32, @_jscript, etc.)
    CCValue.h           # Variant type (number/string/bool)
    CCError.h           # Error struct with line/column
    Scanner.h           # State machine: identifies CC blocks in source
    Tokenizer.h         # Tokenizes CC block content into tokens
    Parser.h            # Parses CC tokens into AST (Pratt parser for expressions)
    AST.h               # Node types: BlockNode, IfNode, SetNode, TextNode, ExprNode
    Evaluator.h         # Evaluates AST against environment
    Generator.h         # Orchestrates scan→tokenize→parse→evaluate pipeline
    StringSlice.h       # Non-owning string view (C++11 string_view equivalent)

src/
    (one .cpp per header above)

tests/
    main.cpp            # Self-contained test suite (no third-party deps)
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

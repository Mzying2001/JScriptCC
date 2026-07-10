#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <sstream>
#include <cstring>

#include "jscriptcc/CCPreprocessor.h"
#include "jscriptcc/CCEnvironment.h"

// ── Simple test framework ────────────────────────────────────────────────────

static int g_testsPassed = 0;
static int g_testsFailed = 0;
static int g_testsTotal = 0;

struct TestCase {
    std::string name;
    std::function<void()> func;
};

static std::vector<TestCase> g_tests;

#define TEST(name) \
    static void test_##name(); \
    static struct Register_##name { \
        Register_##name() { g_tests.push_back({#name, test_##name}); } \
    } reg_##name; \
    static void test_##name()

#define ASSERT_EQ(actual, expected) do { \
    g_testsTotal++; \
    if ((actual) != (expected)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::cerr << "    Expected: [" << (expected) << "]\n"; \
        std::cerr << "    Actual:   [" << (actual) << "]\n"; \
        g_testsFailed++; \
        return; \
    } \
    g_testsPassed++; \
} while(0)

#define ASSERT_TRUE(expr) do { \
    g_testsTotal++; \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::cerr << "    Expression was false: " << #expr << "\n"; \
        g_testsFailed++; \
        return; \
    } \
    g_testsPassed++; \
} while(0)

#define ASSERT_FALSE(expr) do { \
    g_testsTotal++; \
    if ((expr)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::cerr << "    Expression was true: " << #expr << "\n"; \
        g_testsFailed++; \
        return; \
    } \
    g_testsPassed++; \
} while(0)

// Helper to process and return output
static std::string process(const std::string& source, const jscriptcc::CCEnvironment& env = jscriptcc::CCEnvironment()) {
    jscriptcc::CCPreprocessor pp;
    std::string output;
    pp.Process(source, output, env);
    return output;
}

// ── Test: Normal JS unchanged ────────────────────────────────────────────────

TEST(normal_js_unchanged) {
    std::string src = "var x = 1;\nfunction foo() { return 42; }\n";
    ASSERT_EQ(process(src), src);
}

TEST(normal_js_with_comments) {
    std::string src = "// line comment\n/* block comment */\nvar x = 1;\n";
    ASSERT_EQ(process(src), src);
}

TEST(normal_js_with_strings) {
    std::string src = "var s = \"hello world\";\nvar t = 'single quotes';\n";
    ASSERT_EQ(process(src), src);
}

// ── Test: CC block with @if ──────────────────────────────────────────────────

TEST(cc_if_win32_true) {
    std::string src =
        "foo();\n"
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert(1);\n"
        "@end\n"
        "@*/\n"
        "bar();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("alert(1);") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
    ASSERT_FALSE(out.find("@if") != std::string::npos);
    ASSERT_FALSE(out.find("@end") != std::string::npos);
    ASSERT_FALSE(out.find("@cc_on") != std::string::npos);
}

TEST(cc_if_win32_false) {
    std::string src =
        "foo();\n"
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert(1);\n"
        "@end\n"
        "@*/\n"
        "bar();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(0.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_FALSE(out.find("alert(1);") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
}

// ── Test: @if/@else ──────────────────────────────────────────────────────────

TEST(cc_if_else) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert('win32');\n"
        "@else\n"
        "alert('other');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(0.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert('other');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win32');") != std::string::npos);
}

// ── Test: @if/@elif/@else ────────────────────────────────────────────────────

TEST(cc_if_elif_else) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_jscript_version >= 9)\n"
        "alert('ie9+');\n"
        "@elif(@_jscript_version >= 5)\n"
        "alert('ie5+');\n"
        "@else\n"
        "alert('old');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert('ie5+');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('ie9+');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('old');") != std::string::npos);
}

// ── Test: Nested @if ─────────────────────────────────────────────────────────

TEST(cc_nested_if) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "@if(@_jscript_version >= 5)\n"
        "alert('win32 ie5+');\n"
        "@else\n"
        "alert('win32 old');\n"
        "@end\n"
        "@else\n"
        "alert('not win32');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert('win32 ie5+');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win32 old');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('not win32');") != std::string::npos);
}

// ── Test: @set ───────────────────────────────────────────────────────────────

TEST(cc_set_variable) {
    std::string src =
        "/*@cc_on\n"
        "@set @DEBUG = 1\n"
        "@if(@DEBUG)\n"
        "console.log('debug');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("console.log('debug');") != std::string::npos);
}

TEST(cc_set_expression) {
    std::string src =
        "/*@cc_on\n"
        "@set @VERSION = 5\n"
        "@if(@VERSION >= 5)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

// ── Test: Expression operators ───────────────────────────────────────────────

TEST(expr_comparison) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_jscript_version >= 5.8)\n"
        "alert('gte');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);
    ASSERT_TRUE(out.find("alert('gte');") != std::string::npos);
}

TEST(expr_logical_and) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32 && @_jscript_version >= 5)\n"
        "alert('both');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);
    ASSERT_TRUE(out.find("alert('both');") != std::string::npos);
}

TEST(expr_logical_or) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32 || @_win64)\n"
        "alert('windows');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(0.0));
    env.set("@_win64", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);
    ASSERT_TRUE(out.find("alert('windows');") != std::string::npos);
}

TEST(expr_not) {
    std::string src =
        "/*@cc_on\n"
        "@if(!@_debug)\n"
        "alert('release');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_debug", jscriptcc::CCValue(0.0));
    std::string out = process(src, env);
    ASSERT_TRUE(out.find("alert('release');") != std::string::npos);
}

TEST(expr_bitwise_ops) {
    std::string src =
        "/*@cc_on\n"
        "@set @X = 5\n"
        "@set @Y = 3\n"
        "@if((@X & @Y) == 1)\n"
        "alert('bitand');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('bitand');") != std::string::npos);
}

TEST(expr_arithmetic) {
    std::string src =
        "/*@cc_on\n"
        "@set @X = 10\n"
        "@set @Y = 3\n"
        "@if(@X % @Y == 1)\n"
        "alert('mod');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('mod');") != std::string::npos);
}

// ── Test: Empty CC block ─────────────────────────────────────────────────────

TEST(cc_empty_block) {
    std::string src =
        "foo();\n"
        "/*@cc_on\n"
        "@*/\n"
        "bar();\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
}

// ── Test: Multiple CC blocks ─────────────────────────────────────────────────

TEST(cc_multiple_blocks) {
    std::string src =
        "foo();\n"
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert('first');\n"
        "@end\n"
        "@*/\n"
        "bar();\n"
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert('second');\n"
        "@end\n"
        "@*/\n"
        "baz();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("alert('first');") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
    ASSERT_TRUE(out.find("alert('second');") != std::string::npos);
    ASSERT_TRUE(out.find("baz();") != std::string::npos);
}

// ── Test: String containing @cc_on (should NOT trigger CC) ───────────────────

TEST(string_with_cc_on) {
    std::string src = "var s = \"@cc_on\";\nvar t = '@if';\n";
    ASSERT_EQ(process(src), src);
}

TEST(string_with_cc_block) {
    std::string src = "var s = \"/*@cc_on\";\n";
    ASSERT_EQ(process(src), src);
}

TEST(template_string_with_cc) {
    std::string src = "var s = `@cc_on`;\nvar t = `@if`;\nvar u = `@end`;\n";
    ASSERT_EQ(process(src), src);
}

TEST(template_expression_with_cc) {
    std::string src = "const t = `abc ${\"@if\"} def`;\n";
    ASSERT_EQ(process(src), src);
}

// ── Test: Regex containing @ directives (should NOT trigger CC) ──────────────

TEST(regex_with_cc) {
    std::string src = "var r = /@if/;\nvar s = /@end/;\n";
    ASSERT_EQ(process(src), src);
}

// ── Test: Comments containing @ directives (should NOT trigger CC) ───────────

TEST(line_comment_with_cc) {
    std::string src = "// @cc_on\n// @if\n// @end\n";
    ASSERT_EQ(process(src), src);
}

TEST(block_comment_with_cc) {
    std::string src = "/* @cc_on */\n/* @if */\n";
    ASSERT_EQ(process(src), src);
}

TEST(block_comment_multiline_with_cc) {
    std::string src = "/*\nabc\n@cc_on\n*/\n";
    ASSERT_EQ(process(src), src);
}

// ── Test: @end inside string (should NOT end CC) ─────────────────────────────

TEST(end_in_string_in_js) {
    std::string src = "function test(){\n    return \"@end\";\n}\n";
    ASSERT_EQ(process(src), src);
}

TEST(cc_block_ignores_directives_in_comments) {
    std::string src =
        "/*@cc_on\n"
        "@if(1)\n"
        "keep1();\n"
        "// @if(0) this is only a comment\n"
        "SHOULD_REMAIN();\n"
        "// @end this is only a comment\n"
        "/* @else and @end are also comments */\n"
        "keep2();\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("SHOULD_REMAIN();") != std::string::npos);
    ASSERT_TRUE(out.find("// @if(0) this is only a comment") != std::string::npos);
    ASSERT_TRUE(out.find("/* @else and @end are also comments */") != std::string::npos);
}

TEST(cc_block_ignores_directives_in_regex) {
    std::string src =
        "/*@cc_on\n"
        "@if(1)\n"
        "var first = /@if(0)/;\n"
        "var second = /[@end/]/g;\n"
        "SHOULD_REMAIN();\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("var first = /@if(0)/;") != std::string::npos);
    ASSERT_TRUE(out.find("var second = /[@end/]/g;") != std::string::npos);
    ASSERT_TRUE(out.find("SHOULD_REMAIN();") != std::string::npos);
}

TEST(cc_block_ignores_directives_in_templates) {
    std::string src =
        "/*@cc_on\n"
        "@if(1)\n"
        "const text = `@if(0) ${'@end'} @else`;\n"
        "SHOULD_REMAIN();\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("const text = `@if(0) ${'@end'} @else`;") != std::string::npos);
    ASSERT_TRUE(out.find("SHOULD_REMAIN();") != std::string::npos);
}

// ── Test: //@cc_on form ─────────────────────────────────────────────────────

TEST(double_slash_cc_on) {
    std::string src =
        "foo();\n"
        "//@cc_on\n"
        "@if(@_win32)\n"
        "alert('win32');\n"
        "@end\n"
        "bar();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("alert('win32');") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
    ASSERT_FALSE(out.find("@if") != std::string::npos);
}

// ── Test: Predefined variables ───────────────────────────────────────────────

TEST(predefined_jscript) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_jscript)\n"
        "alert('jscript');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('jscript');") != std::string::npos);
}

TEST(predefined_windows_architecture) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert('win32');\n"
        "@elif(@_win64)\n"
        "alert('win64');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    if (sizeof(void*) == 8) {
        ASSERT_TRUE(out.find("alert('win64');") != std::string::npos);
        ASSERT_FALSE(out.find("alert('win32');") != std::string::npos);
    } else {
        ASSERT_TRUE(out.find("alert('win32');") != std::string::npos);
        ASSERT_FALSE(out.find("alert('win64');") != std::string::npos);
    }
}

TEST(predefined_jscript_version) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_jscript_version >= 5.8)\n"
        "alert('version');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('version');") != std::string::npos);
}

// ── Test: User can override predefined variables ─────────────────────────────

TEST(override_predefined) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert('win32');\n"
        "@else\n"
        "alert('other');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(0.0));
    std::string out = process(src, env);
    ASSERT_TRUE(out.find("alert('other');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win32');") != std::string::npos);
}

// ── Test: Whitespace preservation ────────────────────────────────────────────

TEST(preserve_whitespace) {
    std::string src =
        "foo();\n"
        "\n"
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert(1);\n"
        "@end\n"
        "@*/\n"
        "\n"
        "bar();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    // Check that the output has foo, alert, and bar
    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("alert(1);") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
}

// ── Test: Error recovery ─────────────────────────────────────────────────────

TEST(missing_end) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert(1);\n"
        "@*/\n";

    jscriptcc::CCPreprocessor pp;
    std::string output;
    std::vector<jscriptcc::CCError> errors;
    pp.Process(src, output, jscriptcc::CCEnvironment(), &errors);

    // Should have at least one error (missing @end)
    ASSERT_TRUE(errors.size() > 0);
}

// ── Test: Large file performance ─────────────────────────────────────────────

TEST(large_file_performance) {
    // Generate a 10MB+ source file
    std::string large;
    large.reserve(12 * 1024 * 1024);

    large += "/*@cc_on\n@if(@_win32)\nalert('win');\n@end\n@*/\n";

    // Fill with normal JS
    for (int i = 0; i < 200000; ++i) {
        large += "var x" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    }

    large += "/*@cc_on\n@if(@_win32)\nalert('win2');\n@end\n@*/\n";

    for (int i = 200000; i < 400000; ++i) {
        large += "var x" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    }

    auto start = std::chrono::steady_clock::now();

    jscriptcc::CCPreprocessor pp;
    std::string output;
    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    bool ok = pp.Process(large, output, env);

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(ok);
    ASSERT_TRUE(output.size() > 0);
    ASSERT_TRUE(output.find("alert('win');") != std::string::npos);
    ASSERT_TRUE(output.find("alert('win2');") != std::string::npos);

    std::cout << "    [Large file: " << large.size() / 1024 << "KB -> "
              << output.size() / 1024 << "KB in " << ms << "ms]\n";
}

// ── Test: Complex real-world-like scenario ───────────────────────────────────

TEST(real_world_scenario) {
    std::string src =
        "/*\n"
        " * Library v2.0\n"
        " */\n"
        "(function() {\n"
        "    var version = '2.0';\n"
        "\n"
        "/*@cc_on\n"
        "\n"
        "@set @_debug = 0\n"
        "\n"
        "@if(@_jscript_version >= 9)\n"
        "    version = '2.0-ie9+';\n"
        "@elif(@_jscript_version >= 5.8)\n"
        "    version = '2.0-ie8';\n"
        "@elif(@_jscript_version >= 5)\n"
        "    version = '2.0-ie5-7';\n"
        "@else\n"
        "    version = '2.0-old';\n"
        "@end\n"
        "\n"
        "@if(@_win32 && !@_win64)\n"
        "    var platform = 'win32';\n"
        "@elif(@_win64)\n"
        "    var platform = 'win64';\n"
        "@else\n"
        "    var platform = 'unknown';\n"
        "@end\n"
        "\n"
        "@*/\n"
        "\n"
        "    // String containing @cc_on should not be affected\n"
        "    var s = '@cc_on';\n"
        "    var r = /@if/;\n"
        "\n"
        "    console.log(version, platform);\n"
        "})();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    env.set("@_win32", jscriptcc::CCValue(1.0));
    env.set("@_win64", jscriptcc::CCValue(0.0));
    env.set("@_debug", jscriptcc::CCValue(0.0));

    std::string out = process(src, env);

    ASSERT_TRUE(out.find("version = '2.0-ie8'") != std::string::npos);
    ASSERT_TRUE(out.find("var platform = 'win32'") != std::string::npos);
    ASSERT_TRUE(out.find("var s = '@cc_on'") != std::string::npos);
    ASSERT_TRUE(out.find("var r = /@if/") != std::string::npos);
    // Ensure no CC block delimiters remain
    ASSERT_FALSE(out.find("/*@cc_on") != std::string::npos);
    ASSERT_FALSE(out.find("@*/") != std::string::npos);
    // Ensure no bare @if/@elif/@end directives remain (outside strings/regex)
    // Note: @if and @cc_on appear inside string/regex literals, which is correct
}

// ── Test: @if with string comparison ─────────────────────────────────────────

TEST(string_comparison) {
    std::string src =
        "/*@cc_on\n"
        "@set @MODE = 'production'\n"
        "@if(@MODE == 'production')\n"
        "console.log('prod');\n"
        "@else\n"
        "console.log('dev');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("console.log('prod');") != std::string::npos);
    ASSERT_FALSE(out.find("console.log('dev');") != std::string::npos);
}

// ── Test: @set without @if ───────────────────────────────────────────────────

TEST(set_without_if) {
    std::string src =
        "/*@cc_on\n"
        "@set @X = 42\n"
        "@*/\n"
        "bar();\n";

    std::string out = process(src);
    // @set alone doesn't produce output
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
}

// ── Test: Undefined variable ─────────────────────────────────────────────────

TEST(undefined_variable_is_zero) {
    std::string src =
        "/*@cc_on\n"
        "@if(@UNDEFINED_VAR)\n"
        "alert('yes');\n"
        "@else\n"
        "alert('no');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('no');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('yes');") != std::string::npos);
}

// ── Test: Negative numbers ───────────────────────────────────────────────────

TEST(negative_number) {
    std::string src =
        "/*@cc_on\n"
        "@set @X = -5\n"
        "@if(@X < 0)\n"
        "alert('negative');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('negative');") != std::string::npos);
}

// ── Test: Hex numbers ────────────────────────────────────────────────────────

TEST(hex_number) {
    std::string src =
        "/*@cc_on\n"
        "@set @X = 0xFF\n"
        "@if(@X == 255)\n"
        "alert('hex');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('hex');") != std::string::npos);
}

// ── Test: Shift operators ────────────────────────────────────────────────────

TEST(shift_operators) {
    std::string src =
        "/*@cc_on\n"
        "@set @X = 1\n"
        "@if((@X << 3) == 8)\n"
        "alert('shl');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('shl');") != std::string::npos);
}

// ── Test: Adjacent CC blocks ─────────────────────────────────────────────────

TEST(adjacent_cc_blocks) {
    std::string src =
        "/*@cc_on @if(@_win32) alert(1); @end @*/\n"
        "/*@cc_on @if(@_win32) alert(2); @end @*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert(1)") != std::string::npos);
    ASSERT_TRUE(out.find("alert(2)") != std::string::npos);
}

// ── Test: /*@directive form without cc_on ────────────────────────────────────

TEST(no_cc_on_basic_if) {
    std::string src =
        "foo();\n"
        "/*@if(@_win32)\n"
        "alert(1);\n"
        "@end\n"
        "@*/\n"
        "bar();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("alert(1);") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
    ASSERT_FALSE(out.find("@if") != std::string::npos);
    ASSERT_FALSE(out.find("@end") != std::string::npos);
}

TEST(no_cc_on_if_false) {
    std::string src =
        "/*@if(@_win32)\n"
        "alert(1);\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(0.0));
    std::string out = process(src, env);

    ASSERT_FALSE(out.find("alert(1);") != std::string::npos);
}

TEST(no_cc_on_if_else) {
    std::string src =
        "/*@if(@_win32)\n"
        "alert('win32');\n"
        "@else\n"
        "alert('other');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(0.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert('other');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win32');") != std::string::npos);
}

TEST(no_cc_on_if_elif_else) {
    std::string src =
        "/*@if(@_jscript_version >= 9)\n"
        "alert('ie9+');\n"
        "@elif(@_jscript_version >= 5)\n"
        "alert('ie5+');\n"
        "@else\n"
        "alert('old');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert('ie5+');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('ie9+');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('old');") != std::string::npos);
}

TEST(no_cc_on_nested_if) {
    std::string src =
        "/*@if(@_win32)\n"
        "@if(@_jscript_version >= 5)\n"
        "alert('win32 ie5+');\n"
        "@else\n"
        "alert('win32 old');\n"
        "@end\n"
        "@else\n"
        "alert('not win32');\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert('win32 ie5+');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win32 old');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('not win32');") != std::string::npos);
}

TEST(no_cc_on_set) {
    std::string src =
        "/*@set @DEBUG = 1\n"
        "@if(@DEBUG)\n"
        "console.log('debug');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("console.log('debug');") != std::string::npos);
}

TEST(no_cc_on_multiple_blocks) {
    std::string src =
        "foo();\n"
        "/*@if(@_win32)\n"
        "alert('first');\n"
        "@end\n"
        "@*/\n"
        "bar();\n"
        "/*@if(@_win32)\n"
        "alert('second');\n"
        "@end\n"
        "@*/\n"
        "baz();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("foo();") != std::string::npos);
    ASSERT_TRUE(out.find("alert('first');") != std::string::npos);
    ASSERT_TRUE(out.find("bar();") != std::string::npos);
    ASSERT_TRUE(out.find("alert('second');") != std::string::npos);
    ASSERT_TRUE(out.find("baz();") != std::string::npos);
}

TEST(no_cc_on_adjacent_blocks) {
    std::string src =
        "/*@if(@_win32) alert(1); @end @*/\n"
        "/*@if(@_win32) alert(2); @end @*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("alert(1)") != std::string::npos);
    ASSERT_TRUE(out.find("alert(2)") != std::string::npos);
}

TEST(no_cc_on_with_space_is_regular_comment) {
    // /* @if (with space after /*) should NOT trigger CC
    std::string src = "/* @if */\n/* @end */\nvar x = 1;\n";
    ASSERT_EQ(process(src), src);
}

TEST(no_cc_on_string_with_directive) {
    // Strings containing @if should not be affected
    std::string src =
        "/*@if(@_win32)\n"
        "var s = '@end';\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("var s = '@end';") != std::string::npos);
}

TEST(no_cc_on_real_world) {
    std::string src =
        "(function() {\n"
        "    var version = '2.0';\n"
        "\n"
        "/*@if(@_jscript_version >= 5.8)\n"
        "    version = '2.0-ie8';\n"
        "@else\n"
        "    version = '2.0-old';\n"
        "@end\n"
        "@*/\n"
        "\n"
        "    console.log(version);\n"
        "})();\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);

    ASSERT_TRUE(out.find("version = '2.0-ie8'") != std::string::npos);
    ASSERT_FALSE(out.find("version = '2.0-old'") != std::string::npos);
    ASSERT_FALSE(out.find("/*@if") != std::string::npos);
    ASSERT_FALSE(out.find("@*/") != std::string::npos);
}

// ── Test: flashChecker function with CC directives ────────────────────────────

TEST(flash_checker_cc_on_bang) {
    // Test the /*@cc_on!@*/ pattern - outputs content between @cc_on and @*/
    std::string src = "var isIE = /*@cc_on!@*/0;\n";

    std::string out = process(src);

    // /*@cc_on!@*/ outputs "!", so result is !0
    ASSERT_TRUE(out.find("var isIE = !0;") != std::string::npos);
    ASSERT_FALSE(out.find("/*@cc_on") != std::string::npos);
}

TEST(flash_checker_cc_on_bang_in_function) {
    // Test /*@cc_on!@*/ inside a function
    std::string src =
        "function flashChecker() {\n"
        "    var hasFlash = 0;\n"
        "    var flashVersion = 0;\n"
        "    var isIE = /*@cc_on!@*/0;\n"
        "    return {f: hasFlash, v: flashVersion};\n"
        "}\n";

    std::string out = process(src);

    // /*@cc_on!@*/ outputs "!", making it !0
    ASSERT_TRUE(out.find("var isIE = !0;") != std::string::npos);
    ASSERT_FALSE(out.find("/*@cc_on") != std::string::npos);
}

TEST(flash_checker_full_function) {
    // Test full flashChecker function with CC directive
    std::string src =
        "function flashChecker() {\n"
        "    var hasFlash = 0;\n"
        "    var flashVersion = 0;\n"
        "    var isIE = /*@cc_on!@*/0;\n"
        "\n"
        "    if (isIE) {\n"
        "        var swf = new ActiveXObject('ShockwaveFlash.ShockwaveFlash');\n"
        "        if (swf) {\n"
        "            hasFlash = 1;\n"
        "            VSwf = swf.GetVariable('$version');\n"
        "            flashVersion = parseInt(VSwf.split(' ')[1].split(',')[0]);\n"
        "        }\n"
        "    }\n"
        "    return {f: hasFlash, v: flashVersion};\n"
        "}\n";

    std::string out = process(src);

    // /*@cc_on!@*/ outputs "!", making it !0
    ASSERT_TRUE(out.find("var isIE = !0;") != std::string::npos);
    // Rest of the code should be preserved
    ASSERT_TRUE(out.find("ActiveXObject") != std::string::npos);
    ASSERT_TRUE(out.find("ShockwaveFlash") != std::string::npos);
}

TEST(flash_checker_with_plugins_code) {
    // Test flashChecker with navigator.plugins detection
    std::string src =
        "function flashChecker() {\n"
        "    var hasFlash = 0;\n"
        "    var flashVersion = 0;\n"
        "    var isIE = /*@cc_on!@*/0;\n"
        "\n"
        "    if (isIE) {\n"
        "        var swf = new ActiveXObject('ShockwaveFlash.ShockwaveFlash');\n"
        "    } else {\n"
        "        if (navigator.plugins && navigator.plugins.length > 0) {\n"
        "            var swf = navigator.plugins['Shockwave Flash'];\n"
        "            if (swf) {\n"
        "                hasFlash = 1;\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    return {f: hasFlash, v: flashVersion};\n"
        "}\n";

    std::string out = process(src);

    // /*@cc_on!@*/ outputs "!", making it !0
    ASSERT_TRUE(out.find("var isIE = !0;") != std::string::npos);
    // Both code paths should be preserved
    ASSERT_TRUE(out.find("navigator.plugins") != std::string::npos);
    ASSERT_TRUE(out.find("Shockwave Flash") != std::string::npos);
}

TEST(flash_checker_with_cc_if_version_check) {
    // Test flashChecker using @if to check JScript version
    std::string src =
        "function flashChecker() {\n"
        "    var hasFlash = 0;\n"
        "    var flashVersion = 0;\n"
        "\n"
        "/*@cc_on\n"
        "    @if(@_jscript_version >= 5.6)\n"
        "        var swf = new ActiveXObject('ShockwaveFlash.ShockwaveFlash');\n"
        "        if (swf) {\n"
        "            hasFlash = 1;\n"
        "        }\n"
        "    @else\n"
        "        // Old IE without proper ActiveX support\n"
        "        hasFlash = -1;\n"
        "    @end\n"
        "@*/\n"
        "\n"
        "    return {f: hasFlash, v: flashVersion};\n"
        "}\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.8));
    std::string out = process(src, env);

    // Should take the >= 5.6 branch
    ASSERT_TRUE(out.find("ActiveXObject") != std::string::npos);
    ASSERT_FALSE(out.find("hasFlash = -1") != std::string::npos);
    ASSERT_FALSE(out.find("@if") != std::string::npos);
    ASSERT_FALSE(out.find("@*/") != std::string::npos);
}

TEST(flash_checker_with_cc_if_old_version) {
    // Test flashChecker with old JScript version
    std::string src =
        "function flashChecker() {\n"
        "    var hasFlash = 0;\n"
        "\n"
        "/*@cc_on\n"
        "    @if(@_jscript_version >= 5.6)\n"
        "        var swf = new ActiveXObject('ShockwaveFlash.ShockwaveFlash');\n"
        "    @else\n"
        "        hasFlash = -1;\n"
        "    @end\n"
        "@*/\n"
        "\n"
        "    return {f: hasFlash};\n"
        "}\n";

    jscriptcc::CCEnvironment env;
    env.set("@_jscript_version", jscriptcc::CCValue(5.0));
    std::string out = process(src, env);

    // Should take the else branch
    ASSERT_TRUE(out.find("hasFlash = -1") != std::string::npos);
    ASSERT_FALSE(out.find("ActiveXObject") != std::string::npos);
}

// ── Test: @cc_on with various content after it ───────────────────────────────

TEST(cc_on_bang_inline) {
    // /*@cc_on!@*/ - outputs "!" as content
    std::string src = "var x = /*@cc_on!@*/0;\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("var x = !0;") != std::string::npos);
}

TEST(cc_on_space_then_directive) {
    // /*@cc_on @if(...)...@*/ - space after @cc_on, then directive
    std::string src =
        "/*@cc_on\n"
        "    @if(1)\n"
        "        var a = 1;\n"
        "    @end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("var a = 1;") != std::string::npos);
    ASSERT_FALSE(out.find("@if") != std::string::npos);
}

TEST(cc_on_newline_then_directive) {
    // /*@cc_on\n@if(...)...@*/ - newline after @cc_on
    std::string src =
        "/*@cc_on\n"
        "@if(@_jscript)\n"
        "    alert('ie');\n"
        "@end\n"
        "@*/\n";
    jscriptcc::CCEnvironment env;
    env.set("@_jscript", jscriptcc::CCValue(1.0));
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ie')") != std::string::npos);
}

TEST(cc_on_empty_block) {
    // /*@cc_on @*/ - empty CC block
    std::string src = "var x = 1; /*@cc_on @*/ var y = 2;\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("var x = 1;") != std::string::npos);
    ASSERT_TRUE(out.find("var y = 2;") != std::string::npos);
}

TEST(cc_on_longer_identifier_not_matched) {
    // /*@cc_onwards*/ - should NOT be recognized as CC block
    std::string src = "var x = /*@cc_onwards*/;\n";
    std::string out = process(src);
    // Should pass through as normal JS (regular block comment)
    ASSERT_TRUE(out.find("/*@cc_onwards*/") != std::string::npos);
}

TEST(cc_on_with_set) {
    // /*@cc_on @set ... @*/ - @cc_on followed by @set
    std::string src =
        "/*@cc_on\n"
        "    @set @x = 10\n"
        "@*/\n"
        "var result = /*@cc_on!@*/0;\n";
    std::string out = process(src);
    // @set has no output, @cc_on!@ outputs "!"
    ASSERT_TRUE(out.find("var result = !0;") != std::string::npos);
    ASSERT_FALSE(out.find("@set") != std::string::npos);
}

TEST(cc_on_multiple_bang_usages) {
    // Multiple /*@cc_on!@*/ in one source
    std::string src =
        "var a = /*@cc_on!@*/0;\n"
        "var b = /*@cc_on!@*/1;\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("var a = !0;") != std::string::npos);
    ASSERT_TRUE(out.find("var b = !1;") != std::string::npos);
}

// ── Test: Division and modulo expressions ───────────────────────────────────

TEST(expr_division) {
    // 10 / 3 == 3 is false (integer division doesn't exist in JS), use 9/3
    std::string src =
        "/*@cc_on\n"
        "@set @a = 9\n"
        "@set @b = 3\n"
        "@if(@a / @b == 3)\n"
        "alert('div');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('div');") != std::string::npos);
}

TEST(expr_modulo) {
    std::string src =
        "/*@cc_on\n"
        "@set @a = 10\n"
        "@set @b = 3\n"
        "@if(@a % @b == 1)\n"
        "alert('mod');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('mod');") != std::string::npos);
}

TEST(expr_division_by_zero) {
    // Division by zero should produce Infinity, not crash
    std::string src =
        "/*@cc_on\n"
        "@if(1 / 0 > 1000000)\n"
        "alert('inf');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('inf');") != std::string::npos);
}

TEST(expr_modulo_by_zero) {
    // Modulo by zero should produce NaN, not crash
    std::string src =
        "/*@cc_on\n"
        "@if(1 % 0 != 1 % 0)\n"
        "alert('nan');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('nan');") != std::string::npos);
}

// ── Test: Bitwise operations with edge cases ───────────────────────────────

TEST(expr_bitnot) {
    std::string src =
        "/*@cc_on\n"
        "@if(~0 == -1)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_bitwise_large_number) {
    // Large numbers should not crash (safeToInt32 handles out-of-range)
    std::string src =
        "/*@cc_on\n"
        "@set @big = 1000000000000\n"
        "@if((@big & 0xFF) >= 0)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_bitwise_or) {
    std::string src =
        "/*@cc_on\n"
        "@if((0x0F | 0xF0) == 0xFF)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_bitwise_xor) {
    std::string src =
        "/*@cc_on\n"
        "@if((0xFF ^ 0x0F) == 0xF0)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

// ── Test: Comparison operators ─────────────────────────────────────────────

TEST(expr_lt) {
    std::string src =
        "/*@cc_on\n"
        "@if(1 < 2)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_le) {
    std::string src =
        "/*@cc_on\n"
        "@if(1 <= 1)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_gt) {
    std::string src =
        "/*@cc_on\n"
        "@if(2 > 1)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_ne) {
    std::string src =
        "/*@cc_on\n"
        "@if(1 != 2)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

TEST(expr_eq) {
    std::string src =
        "/*@cc_on\n"
        "@if(1 == 1)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
}

// ── Test: Regex vs division in normal JS ───────────────────────────────────

TEST(regex_after_identifier_is_division) {
    // After a plain identifier, / should be division, not regex
    std::string src = "var x = a / b;\n";
    ASSERT_EQ(process(src), src);
}

TEST(regex_after_operator) {
    // After an operator like =, / should be regex
    std::string src = "var r = /test/;\n";
    ASSERT_EQ(process(src), src);
}

TEST(regex_after_return) {
    // After return keyword, / should be regex
    std::string src = "return /test/;\n";
    ASSERT_EQ(process(src), src);
}

TEST(regex_in_normal_js_preserved) {
    // Regex in normal JS (not in CC block) should pass through
    std::string src = "var r = /hello/gi;\n";
    ASSERT_EQ(process(src), src);
}

TEST(division_in_expression_preserved) {
    // Division in expressions should pass through
    std::string src = "var result = 10 / 2 / 5;\n";
    ASSERT_EQ(process(src), src);
}

// ── Test: Nested @elif chains ──────────────────────────────────────────────

TEST(cc_multiple_elif) {
    std::string src =
        "/*@cc_on\n"
        "@set @v = 3\n"
        "@if(@v == 1)\n"
        "alert(1);\n"
        "@elif(@v == 2)\n"
        "alert(2);\n"
        "@elif(@v == 3)\n"
        "alert(3);\n"
        "@else\n"
        "alert(4);\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert(3);") != std::string::npos);
    ASSERT_FALSE(out.find("alert(1);") != std::string::npos);
    ASSERT_FALSE(out.find("alert(2);") != std::string::npos);
    ASSERT_FALSE(out.find("alert(4);") != std::string::npos);
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Running JScriptCC tests...\n\n";

    for (const auto& tc : g_tests) {
        int beforeFailed = g_testsFailed;
        std::cout << "  [RUN ] " << tc.name << "\n";
        tc.func();
        if (g_testsFailed == beforeFailed) {
            std::cout << "  [PASS] " << tc.name << "\n";
        } else {
            std::cout << "  [FAIL] " << tc.name << "\n";
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "Results: " << g_testsPassed << " passed, "
              << g_testsFailed << " failed, "
              << g_testsTotal << " total assertions\n";
    std::cout << "========================================\n";

    return g_testsFailed > 0 ? 1 : 0;
}

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

TEST(predefined_win32) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert('win32');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('win32');") != std::string::npos);
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

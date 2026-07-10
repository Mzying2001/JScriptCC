#include "TestSupport.h"

TEST(normal_js_unchanged) {
    std::string src = "var x = 1;\nfunction foo() { return 42; }\n";
    ASSERT_EQ(process(src), src);
}

TEST(normal_js_with_strings) {
    std::string src = "var s = \"hello world\";\nvar t = 'single quotes';\n";
    ASSERT_EQ(process(src), src);
}

TEST(null_empty_input_is_allowed) {
    jscriptcc::CCPreprocessor pp;
    std::string output = "old";
    std::vector<jscriptcc::CCError> errors;
    bool ok = pp.process(nullptr, 0, output, jscriptcc::CCEnvironment(), &errors);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(output.empty());
    ASSERT_TRUE(errors.empty());
}

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

TEST(end_in_string_in_js) {
    std::string src = "function test(){\n    return \"@end\";\n}\n";
    ASSERT_EQ(process(src), src);
}

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

TEST(preserve_crlf_line_endings) {
    std::string src =
        "before();\r\n"
        "/*@cc_on\r\n"
        "@if(1)\r\n"
        "yes();\r\n"
        "@end\r\n"
        "@*/\r\n"
        "after();\r\n";
    std::string expected =
        "before();\r\n"
        "\r\n"
        "yes();\r\n"
        "\r\n"
        "after();\r\n";
    ASSERT_EQ(process(src), expected);
}

TEST(large_input_smoke_test) {
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

    jscriptcc::CCPreprocessor pp;
    std::string output;
    jscriptcc::CCEnvironment env;
    env.set("@_win32", jscriptcc::CCValue(1.0));
    bool ok = pp.process(large, output, env);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(output.size() > 0);
    ASSERT_TRUE(output.find("alert('win');") != std::string::npos);
    ASSERT_TRUE(output.find("alert('win2');") != std::string::npos);
}

// ── Test: Complex real-world-like scenario ───────────────────────────────────

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

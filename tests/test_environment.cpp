#include "TestSupport.h"

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
    ASSERT_TRUE(out.find("alert('win32');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win64');") != std::string::npos);

    jscriptcc::CCEnvironment win64(jscriptcc::TargetArchitecture::Win64);
    out = process(src, win64);
    ASSERT_TRUE(out.find("alert('win64');") != std::string::npos);
    ASSERT_FALSE(out.find("alert('win32');") != std::string::npos);
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

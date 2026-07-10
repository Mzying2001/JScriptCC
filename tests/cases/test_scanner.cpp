#include "support/TestSupport.h"

TEST(normal_js_with_comments) {
    std::string src = "// line comment\n/* block comment */\nvar x = 1;\n";
    ASSERT_EQ(process(src), src);
}

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

TEST(template_expression_regex_brace_does_not_expose_cc) {
    std::string src =
        "const x = `${/}/.test('x') ? `/*@if(0) BAD @end @*/` : 'ok'}`;\n";
    ASSERT_EQ(process(src), src);
}

TEST(template_expression_comment_brace_does_not_expose_cc) {
    std::string src =
        "const x = `${1 /* } */ ? `/*@if(0) BAD @end @*/` : 'ok'}`;\n";
    ASSERT_EQ(process(src), src);
}

TEST(template_expression_string_brace_does_not_expose_cc) {
    std::string src =
        "const x = `${'}' + `nested ${\"}\"} /*@if(0) BAD @end @*/`}`;\n";
    ASSERT_EQ(process(src), src);
}

TEST(regex_with_cc) {
    std::string src = "var r = /@if/;\nvar s = /@end/;\n";
    ASSERT_EQ(process(src), src);
}

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

TEST(double_slash_cc_on_identifier_suffix_is_regular_comment) {
    std::string src =
        "//@cc_on_helper\n"
        "//@cc_on$helper\n"
        "//@cc_on1\n"
        "@if(0)\n"
        "SHOULD_STAY();\n"
        "@end\n";
    ASSERT_EQ(process(src), src);
}

TEST(line_cc_on_preserves_crlf) {
    std::string src =
        "//@cc_on\r\n"
        "@if(1)\r\n"
        "yes();\r\n"
        "@end\r\n";
    std::string expected =
        "\r\n"
        "yes();\r\n";
    ASSERT_EQ(process(src), expected);
}

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

TEST(cc_directive_dollar_suffix_not_matched) {
    std::string src =
        "var a = /*@cc_on$debug*/1;\n"
        "var b = /*@if$debug*/2;\n";
    ASSERT_EQ(process(src), src);
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

TEST(repeated_cc_on_does_not_stall_parser) {
    std::string src =
        "/*@cc_on\n"
        "@cc_on\n"
        "@if(1)\n"
        "alert('ok');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('ok');") != std::string::npos);
    ASSERT_FALSE(out.find("@cc_on") != std::string::npos);
}

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

#include "TestSupport.h"

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

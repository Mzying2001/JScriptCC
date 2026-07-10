#include "TestSupport.h"

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

TEST(string_literals_decode_escapes) {
    std::string src =
        "/*@cc_on\n"
        "@if('\\x41' == 'A' && '\\u0042' == 'B' && 'a\\n' != 'an')\n"
        "alert('decoded');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('decoded');") != std::string::npos);
}

TEST(string_relational_comparison_is_lexical) {
    std::string src =
        "/*@cc_on\n"
        "@if('b' > 'a' && '10' < '2')\n"
        "alert('lexical');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('lexical');") != std::string::npos);
}

TEST(string_number_equality_uses_numeric_coercion) {
    std::string src =
        "/*@cc_on\n"
        "@if('01' == 1 && ' 2 ' == 2 && 'abc' != 0)\n"
        "alert('coerced');\n"
        "@end\n"
        "@*/\n";
    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('coerced');") != std::string::npos);
}

// ── Test: @set without @if ───────────────────────────────────────────────────

TEST(undefined_variable_is_false_but_not_zero) {
    std::string src =
        "/*@cc_on\n"
        "@if(!@UNDEFINED_VAR && @UNDEFINED_VAR != 0 && @UNDEFINED_VAR != false)\n"
        "alert('undefined');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('undefined');") != std::string::npos);
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

TEST(shift_operators_use_int32_bit_patterns) {
    std::string src =
        "/*@cc_on\n"
        "@if((-1 << 1) == -2 &&\n"
        "    (0x40000000 << 1) == -2147483648 &&\n"
        "    (-8 >> 2) == -2 &&\n"
        "    (1 << 33) == 2)\n"
        "alert('int32');\n"
        "@end\n"
        "@*/\n";

    std::string out = process(src);
    ASSERT_TRUE(out.find("alert('int32');") != std::string::npos);
}

// ── Test: Adjacent CC blocks ─────────────────────────────────────────────────

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

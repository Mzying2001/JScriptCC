#include "TestSupport.h"

TEST(null_nonempty_input_returns_error) {
    jscriptcc::CCPreprocessor pp;
    std::string output = "old";
    std::vector<jscriptcc::CCError> errors;
    bool ok = pp.process(nullptr, 1, output, jscriptcc::CCEnvironment(), &errors);
    ASSERT_FALSE(ok);
    ASSERT_TRUE(output.empty());
    ASSERT_EQ(errors.size(), static_cast<std::size_t>(1));
}

// ── Test: CC block with @if ──────────────────────────────────────────────────

TEST(missing_end) {
    std::string src =
        "/*@cc_on\n"
        "@if(@_win32)\n"
        "alert(1);\n"
        "@*/\n";

    jscriptcc::CCPreprocessor pp;
    std::string output;
    std::vector<jscriptcc::CCError> errors;
    pp.process(src, output, jscriptcc::CCEnvironment(), &errors);

    // Should have at least one error (missing @end)
    ASSERT_TRUE(errors.size() > 0);
}

TEST(errors_use_absolute_source_coordinates) {
    std::string src =
        "first();\n"
        "/*@if(1)\n"
        "ok();\n"
        "@end\n"
        "@*/\n"
        "middle();\n"
        "/*@cc_on\n"
        "@if()\n"
        "bad();\n"
        "@end\n"
        "@*/\n";

    jscriptcc::CCPreprocessor pp;
    std::string output;
    std::vector<jscriptcc::CCError> errors;
    bool ok = pp.process(src, output, jscriptcc::CCEnvironment(), &errors);
    ASSERT_FALSE(ok);
    ASSERT_FALSE(errors.empty());
    ASSERT_EQ(errors.front().line, 8);
    ASSERT_EQ(errors.front().column, 5);
}

// ── Test: Large file performance ─────────────────────────────────────────────

#include "support/TestSupport.h"
#include <climits>

namespace {

void appendCodePoint(std::wstring& output, unsigned int codePoint) {
    if (sizeof(wchar_t) == 2 && codePoint > 0xffff) {
        unsigned int value = codePoint - 0x10000;
        output.push_back(static_cast<wchar_t>(0xd800 + (value >> 10)));
        output.push_back(static_cast<wchar_t>(0xdc00 + (value & 0x3ff)));
    } else {
        output.push_back(static_cast<wchar_t>(codePoint));
    }
}

} // namespace

TEST(wide_string_preserves_unicode) {
    std::wstring source = L"var text = '\u4e2d\u6587';\nvar face = '";
    appendCodePoint(source, 0x1f600);
    source += L"';\n";

    jscriptcc::CCPreprocessor pp;
    std::wstring output;
    bool ok = pp.process(source, output);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(output == source);
}

TEST(wide_string_processes_conditional_blocks) {
    std::wstring source =
        L"before();\r\n"
        L"/*@if(@_win32)\r\n"
        L"var selected = '\u4e2d\u6587';\r\n"
        L"@else\r\n"
        L"var rejected = 'other';\r\n"
        L"@end\r\n"
        L"@*/\r\n"
        L"after();\r\n";

    jscriptcc::CCPreprocessor pp;
    std::wstring output;
    bool ok = pp.process(source, output);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(output.find(L"var selected = '\u4e2d\u6587';") != std::wstring::npos);
    ASSERT_TRUE(output.find(L"var rejected") == std::wstring::npos);
    ASSERT_TRUE(output.find(L"before();\r\n") != std::wstring::npos);
    ASSERT_TRUE(output.find(L"after();\r\n") != std::wstring::npos);
}

TEST(wide_pointer_size_preserves_embedded_null) {
    std::wstring source = L"var first = 1;";
    source.push_back(L'\0');
    source += L"var second = 2;";

    jscriptcc::CCPreprocessor pp;
    std::wstring output;
    bool ok = pp.process(source.data(), source.size(), output);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(output == source);
}

TEST(wide_null_empty_input_is_allowed) {
    jscriptcc::CCPreprocessor pp;
    std::wstring output = L"old";
    jscriptcc::CCErrorList errors;
    bool ok = pp.process(
        static_cast<const wchar_t*>(nullptr),
        0,
        output,
        jscriptcc::CCEnvironment(),
        &errors);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(output.empty());
    ASSERT_TRUE(errors.empty());
}

TEST(wide_null_nonempty_input_returns_error) {
    jscriptcc::CCPreprocessor pp;
    std::wstring output = L"old";
    jscriptcc::CCErrorList errors;
    bool ok = pp.process(
        static_cast<const wchar_t*>(nullptr),
        1,
        output,
        jscriptcc::CCEnvironment(),
        &errors);

    ASSERT_FALSE(ok);
    ASSERT_TRUE(output.empty());
    ASSERT_EQ(errors.size(), static_cast<std::size_t>(1));
}

TEST(wide_invalid_unicode_reports_source_position_and_preserves_errors) {
    std::wstring source = L"ok\nX";
    source.push_back(static_cast<wchar_t>(0xd800));

    jscriptcc::CCPreprocessor pp;
    std::wstring output = L"old";
    jscriptcc::CCErrorList errors;
    errors.push_back(jscriptcc::CCError(9, 9, "existing"));
    bool ok = pp.process(source, output, jscriptcc::CCEnvironment(), &errors);

    ASSERT_FALSE(ok);
    ASSERT_TRUE(output == source);
    ASSERT_EQ(errors.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(errors[0].line, 9);
    ASSERT_EQ(errors[0].column, 9);
    ASSERT_EQ(errors[1].line, 2);
    ASSERT_EQ(errors[1].column, 2);
}

TEST(wide_utf32_rejects_out_of_range_code_point) {
#if WCHAR_MAX > 0xffff
    std::wstring source = L"X";
    source.push_back(static_cast<wchar_t>(0x110000));
    jscriptcc::CCPreprocessor pp;
    std::wstring output;
    jscriptcc::CCErrorList errors;
    bool ok = pp.process(source, output, jscriptcc::CCEnvironment(), &errors);

    ASSERT_FALSE(ok);
    ASSERT_TRUE(output == source);
    ASSERT_EQ(errors.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(errors[0].column, 2);
#else
    ASSERT_TRUE(true);
#endif
}

TEST(wide_errors_use_wchar_code_unit_columns) {
    std::wstring source = L"\u4e2d";
    appendCodePoint(source, 0x1f600);
    source += L"/*@if()\n@end\n@*/";
    int expectedColumn = static_cast<int>(source.find(L')')) + 1;
    jscriptcc::CCPreprocessor pp;
    std::wstring output;
    jscriptcc::CCErrorList errors;
    bool ok = pp.process(source, output, jscriptcc::CCEnvironment(), &errors);

    ASSERT_FALSE(ok);
    ASSERT_TRUE(output == source);
    ASSERT_FALSE(errors.empty());
    ASSERT_EQ(errors.front().line, 1);
    ASSERT_EQ(errors.front().column, expectedColumn);
}

TEST(wide_input_and_output_may_alias) {
    std::wstring source = L"/*@if(1)\nyes();\n@end\n@*/\n";
    jscriptcc::CCPreprocessor pp;
    bool ok = pp.process(source, source);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(source.find(L"yes();") != std::wstring::npos);
    ASSERT_TRUE(source.find(L"@if") == std::wstring::npos);
}

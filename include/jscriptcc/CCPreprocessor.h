#pragma once
#include <string>
#include "CCEnvironment.h"
#include "CCError.h"

namespace jscriptcc {

/// Main API: preprocesses JScript Conditional Compilation to plain JavaScript.
///
/// Narrow overloads use UTF-8. Wide overloads use UTF-16 or UTF-32,
/// according to the platform's wchar_t representation.
/// Normal JavaScript is preserved without reformatting.
class CCPreprocessor {
public:
    /// Process source string. Returns true if no fatal errors.
    bool process(
        const std::string& source,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);

    /// Process with explicit data pointer and size.
    bool process(
        const char* data,
        std::size_t size,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);

    /// Process a wide-character source string.
    bool process(
        const std::wstring& source,
        std::wstring& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);

    /// Process with an explicit wide-character data pointer and code-unit count.
    bool process(
        const wchar_t* data,
        std::size_t size,
        std::wstring& output,
        const CCEnvironment& env = CCEnvironment(),
        CCErrorList* errors = nullptr);
};

} // namespace jscriptcc

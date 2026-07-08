#pragma once
#include <string>
#include <vector>
#include "CCEnvironment.h"
#include "CCError.h"

namespace jscriptcc {

/// Main API: preprocesses JScript Conditional Compilation to plain JavaScript.
///
/// Input: UTF-8 JavaScript source with possible @cc_on blocks.
/// Output: UTF-8 JavaScript with CC expanded, normal JS untouched.
class CCPreprocessor {
public:
    /// Process source string. Returns true if no fatal errors.
    bool Process(
        const std::string& source,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        std::vector<CCError>* errors = nullptr);

    /// Process with explicit data pointer and size.
    bool Process(
        const char* data,
        std::size_t size,
        std::string& output,
        const CCEnvironment& env = CCEnvironment(),
        std::vector<CCError>* errors = nullptr);
};

} // namespace jscriptcc

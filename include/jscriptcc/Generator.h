#pragma once
#include <string>
#include <vector>
#include "Scanner.h"
#include "CCEnvironment.h"
#include "CCError.h"

namespace jscriptcc {

/// Generates output JavaScript by processing CC blocks and preserving normal JS.
/// Does not reformat — preserves original whitespace, comments, etc.
class Generator {
public:
    bool generate(
        const char* sourceData,
        std::size_t sourceSize,
        const std::vector<SourceSegment>& segments,
        CCEnvironment& env,
        std::string& output,
        CCErrorList* errors = nullptr);

private:
    void processCCBlock(
        const char* blockData,
        std::size_t blockBegin,
        std::size_t blockEnd,
        CCEnvironment& env,
        std::string& output);

    void processCCOnLine(
        const char* sourceData,
        std::size_t lineBegin,
        std::size_t lineEnd,
        std::size_t sourceSize,
        CCEnvironment& env,
        std::string& output,
        CCErrorList* errors);

    CCErrorList* errors_ = nullptr;
};

} // namespace jscriptcc

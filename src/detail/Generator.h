#pragma once
#include <string>
#include <vector>
#include "Scanner.h"
#include "jscriptcc/CCEnvironment.h"
#include "jscriptcc/CCError.h"

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
        int blockLine,
        int blockColumn,
        CCEnvironment& env,
        std::string& output);

    CCErrorList* errors_ = nullptr;
};

} // namespace jscriptcc

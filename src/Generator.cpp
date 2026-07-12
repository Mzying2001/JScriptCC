#include "detail/Generator.h"
#include "detail/Tokenizer.h"
#include "detail/Parser.h"
#include "detail/Evaluator.h"

namespace jscriptcc {

bool Generator::generate(
    const char* sourceData,
    std::size_t sourceSize,
    const std::vector<SourceSegment>& segments,
    CCEnvironment& env,
    std::string& output,
    CCErrorList* errors)
{
    errors_ = errors;
    output.clear();
    output.reserve(sourceSize);

    std::size_t errorCount = errors_ ? errors_->size() : 0;
    std::size_t sourcePosition = 0;
    int sourceLine = 1;
    int sourceColumn = 1;

    for (const auto& seg : segments) {
        while (sourcePosition < seg.begin) {
            if (sourceData[sourcePosition] == '\n') {
                ++sourceLine;
                sourceColumn = 1;
            } else {
                ++sourceColumn;
            }
            ++sourcePosition;
        }

        switch (seg.type) {
            case SegmentType::NormalJS: {
                // Preserve normal JavaScript verbatim.
                output.append(sourceData + seg.begin, seg.end - seg.begin);
                break;
            }

            case SegmentType::CCBlock: {
                // Process a conditional compilation block.
                processCCBlock(
                    sourceData, seg.begin, seg.end, sourceLine, sourceColumn, env, output);
                break;
            }

            case SegmentType::CCOnLine: {
                // Omit the activation directive from generated output.
                break;
            }
        }

        while (sourcePosition < seg.end) {
            if (sourceData[sourcePosition] == '\n') {
                ++sourceLine;
                sourceColumn = 1;
            } else {
                ++sourceColumn;
            }
            ++sourcePosition;
        }
    }

    return !errors_ || errors_->size() == errorCount;
}

void Generator::processCCBlock(
    const char* blockData,
    std::size_t blockBegin,
    std::size_t blockEnd,
    int blockLine,
    int blockColumn,
    CCEnvironment& env,
    std::string& output)
{
    // Strip the block delimiters and an optional @cc_on marker.
    std::size_t contentBegin = blockBegin;
    std::size_t contentEnd = blockEnd;
    int contentLine = blockLine;
    int contentColumn = blockColumn;

    if (contentBegin + 2 < blockEnd &&
        blockData[contentBegin] == '/' && blockData[contentBegin + 1] == '*') {
        contentBegin += 2;
        contentColumn += 2;
        const char* p = blockData + contentBegin;
        const char* end = blockData + contentEnd;
        auto advanceContent = [&]() {
            if (*p == '\n') {
                ++contentLine;
                contentColumn = 1;
            } else {
                ++contentColumn;
            }
            ++p;
        };
        while (p < end && (*p == ' ' || *p == '\t')) advanceContent();
        if (p + 6 <= end && std::memcmp(p, "@cc_on", 6) == 0) {
            for (int i = 0; i < 6; ++i) advanceContent();
        } else {
            while (p < end && (std::isalnum(static_cast<unsigned char>(*p)) || *p == '_')) {
                advanceContent();
            }
        }
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) advanceContent();
        if (p < end && *p == '\n') advanceContent();
        contentBegin = static_cast<std::size_t>(p - blockData);
    }

    if (contentEnd >= 3 &&
        blockData[contentEnd - 3] == '@' &&
        blockData[contentEnd - 2] == '*' &&
        blockData[contentEnd - 1] == '/') {
        contentEnd -= 3;
    }

    // Trim trailing horizontal whitespace and one complete line ending before @*/
    while (contentEnd > contentBegin &&
           (blockData[contentEnd - 1] == ' ' || blockData[contentEnd - 1] == '\t')) {
        --contentEnd;
    }
    if (contentEnd > contentBegin && blockData[contentEnd - 1] == '\n') {
        --contentEnd;
        if (contentEnd > contentBegin && blockData[contentEnd - 1] == '\r') --contentEnd;
    } else if (contentEnd > contentBegin && blockData[contentEnd - 1] == '\r') {
        --contentEnd;
    }

    if (contentBegin >= contentEnd) return;

    std::size_t contentSize = contentEnd - contentBegin;
    const char* contentData = blockData + contentBegin;

    // Tokenize and parse the CC content.
    Tokenizer tokenizer;
    tokenizer.tokenize(contentData, contentSize, errors_, contentLine, contentColumn);

    Parser parser;
    auto ast = parser.parse(tokenizer.tokens(), errors_);

    // Evaluate the selected branch and append its output.
    Evaluator evaluator;
    EvalResult result = evaluator.evaluate(*ast, env, errors_);

    if (result.hasOutput) {
        output.append(result.outputText);
    }
}

} // namespace jscriptcc

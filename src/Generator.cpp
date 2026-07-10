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
    output.reserve(sourceSize); // reasonable estimate

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
                // Copy normal JS verbatim
                output.append(sourceData + seg.begin, seg.end - seg.begin);
                break;
            }

            case SegmentType::CCBlock: {
                // Process /*@cc_on ... @*/
                processCCBlock(
                    sourceData, seg.begin, seg.end, sourceLine, sourceColumn, env, output);
                break;
            }

            case SegmentType::CCOnLine: {
                // //@cc_on line — output as a comment (it's a line comment)
                // Actually, in the output we should just remove the //@cc_on line
                // since it's a CC directive. But we could also preserve it as a
                // comment. Let's remove it (the user wants clean JS output).
                // But wait — the @cc_on line itself should not appear in output.
                // The subsequent CC block was already processed.
                // So we just skip this segment.
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

    // Return false if any errors were added during processing
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
    // The block includes /*@cc_on ... @*/
    // We need to extract the content between /*@cc_on and @*/
    // But the content may have already been identified by the scanner.

    // Find the actual CC content: skip /*@cc_on and the newline after it,
    // stop before @*/
    std::size_t contentBegin = blockBegin;
    std::size_t contentEnd = blockEnd;
    int contentLine = blockLine;
    int contentColumn = blockColumn;

    // Skip /*@cc_on (or /*@directive) and the newline after it
    if (contentBegin + 2 < blockEnd &&
        blockData[contentBegin] == '/' && blockData[contentBegin + 1] == '*') {
        contentBegin += 2; // skip /*
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
        // Skip whitespace before @cc_on
        while (p < end && (*p == ' ' || *p == '\t')) advanceContent();
        // Skip @cc_on
        if (p + 6 <= end && std::memcmp(p, "@cc_on", 6) == 0) {
            for (int i = 0; i < 6; ++i) advanceContent();
        } else {
            // Skip directive keyword (@if, @elif, @else, @end, @set)
            while (p < end && (std::isalnum(static_cast<unsigned char>(*p)) || *p == '_')) {
                advanceContent();
            }
        }
        // Skip whitespace and newline after @cc_on / directive
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) advanceContent();
        if (p < end && *p == '\n') advanceContent();
        contentBegin = static_cast<std::size_t>(p - blockData);
    }

    // Find @*/ at the end
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

    if (contentBegin >= contentEnd) return; // empty CC block

    // Tokenize the CC content
    std::size_t contentSize = contentEnd - contentBegin;
    const char* contentData = blockData + contentBegin;

    Tokenizer tokenizer;
    tokenizer.tokenize(contentData, contentSize, errors_, contentLine, contentColumn);

    // Parse
    Parser parser;
    auto ast = parser.parse(tokenizer.tokens(), errors_);

    // Evaluate
    Evaluator evaluator;
    EvalResult result = evaluator.evaluate(*ast, env, errors_);

    // Append evaluated output
    if (result.hasOutput) {
        output.append(result.outputText);
    }
}

} // namespace jscriptcc

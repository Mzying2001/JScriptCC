#pragma once
#include <string>
#include <vector>
#include "jscriptcc/CCError.h"
#include "StringSlice.h"

namespace jscriptcc {

/// Type of source segment identified by the Scanner.
enum class SegmentType {
    NormalJS,     // Regular JavaScript (strings, comments, code)
    CCBlock,      // /*@cc_on ... @*/ block content (including delimiters)
    CCOnLine      // //@cc_on line (activates CC for rest of source)
};

/// A contiguous segment of the source string.
struct SourceSegment {
    SegmentType type;
    std::size_t begin;  // byte offset in source
    std::size_t end;    // byte offset past end (exclusive)
};

/// State machine scanner that identifies Conditional Compilation blocks
/// without parsing JavaScript. Tracks string/comment/regex/template contexts
/// to avoid false positives.
class Scanner {
public:
    /// Scan source and populate segments. Returns false on critical errors.
    bool scan(const char* data, std::size_t size, CCErrorList* errors = nullptr);

    const std::vector<SourceSegment>& segments() const { return segments_; }

private:
    enum class State {
        Default,
        SingleQuoteString,
        DoubleQuoteString,
        TemplateString,
        TemplateExpression,
        RegexLiteral,
        LineComment,
        BlockComment,
        CCBlock,           // Inside /*@cc_on ... @*/
        CCBlockLineComment, // // comment inside CC block
        CCBlockBlockComment // /* comment inside CC block
    };

    // Helpers
    char peek(std::size_t offset = 0) const;
    void adv();
    bool isRegexPosition() const;
    void emitSegment(SegmentType type, std::size_t begin, std::size_t end);
    void addError(int line, int col, const std::string& msg);
    void advanceLine();

    // Skip helpers (called after the opening character is consumed)
    void skipQuotedString(char quote);   // ' or " — advances past closing quote
    void skipTemplateString();           // ` — advances past closing `, handles ${...}
    void skipTemplateExpression();       // { — advances past matching }
    void skipLineComment();              // advances past newline
    void skipBlockComment();             // advances past */
    void skipRegexLiteral();             // advances past /regex/flags

    // State
    const char* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    // Track last non-whitespace, non-comment token for regex detection
    char lastSignificantChar_ = 0;
    bool lastWasOperator_ = false;
    bool lastWasRegexKeyword_ = false;  // true after JS keywords that enable regex (return, typeof, etc.)
    std::string lastIdentifier_;  // accumulates current identifier text

    std::vector<SourceSegment> segments_;
    CCErrorList* errors_ = nullptr;
};

} // namespace jscriptcc

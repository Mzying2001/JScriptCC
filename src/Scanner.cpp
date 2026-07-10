#include "detail/Scanner.h"
#include "CCSyntax.h"
#include <cctype>

namespace jscriptcc {

// ── Helpers ──────────────────────────────────────────────────────────────────

char Scanner::peek(std::size_t offset) const {
    std::size_t p = pos_ + offset;
    return (p < size_) ? data_[p] : '\0';
}

void Scanner::adv() {
    ++pos_;
    ++col_;
}

void Scanner::emitSegment(SegmentType type, std::size_t begin, std::size_t end) {
    if (begin < end) {
        segments_.push_back({type, begin, end});
    }
}

void Scanner::addError(int line, int col, const std::string& msg) {
    if (errors_) {
        errors_->push_back(CCError(line, col, msg));
    }
}

void Scanner::advanceLine() {
    ++line_;
    col_ = 1;
}

void Scanner::skipQuotedString(char quote) {
    // Called after the opening quote is consumed.
    // Advances past the closing quote.
    while (pos_ < size_) {
        if (data_[pos_] == '\\') {
            adv();
            if (pos_ < size_) {
                if (data_[pos_] == '\n') advanceLine();
                adv();
            }
        } else if (data_[pos_] == quote) {
            adv();
            return;
        } else {
            if (data_[pos_] == '\n') advanceLine();
            adv();
        }
    }
}

void Scanner::skipTemplateExpression() {
    // Called after the opening '{' of ${...} is consumed.
    // Advances past the matching '}'.
    int depth = 1;
    bool regexAllowed = true;
    while (pos_ < size_ && depth > 0) {
        char c = data_[pos_];
        if (c == '\'' || c == '"') {
            adv();
            skipQuotedString(c);
            regexAllowed = false;
        } else if (c == '`') {
            adv();
            skipTemplateString();
            regexAllowed = false;
        } else if (c == '/' && peek(1) == '/') {
            adv();
            adv();
            skipLineComment();
        } else if (c == '/' && peek(1) == '*') {
            adv();
            adv();
            skipBlockComment();
        } else if (c == '/' && regexAllowed) {
            adv();
            skipRegexLiteral();
            regexAllowed = false;
        } else if (c == '/') {
            adv();
            regexAllowed = true;
        } else if (c == '{') {
            ++depth;
            adv();
            regexAllowed = true;
        } else if (c == '}') {
            --depth;
            if (depth == 0) { adv(); return; }
            adv();
            regexAllowed = false;
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            std::size_t start = pos_;
            do {
                adv();
            } while (pos_ < size_ && isJSIdentifierChar(data_[pos_]));
            StringSlice word(data_ + start, data_ + pos_);
            regexAllowed = isRegexEnablingKeyword(word.data(), word.size());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            do {
                adv();
            } while (pos_ < size_ &&
                     (std::isalnum(static_cast<unsigned char>(data_[pos_])) ||
                      data_[pos_] == '.' || data_[pos_] == '_'));
            regexAllowed = false;
        } else {
            if (c == '\n') {
                advanceLine();
            } else if (c != ' ' && c != '\t' && c != '\r') {
                if (c == ')' || c == ']') {
                    regexAllowed = false;
                } else if (isRegexPrefixPunctuator(c)) {
                    regexAllowed = true;
                }
            }
            adv();
        }
    }
}

void Scanner::skipTemplateString() {
    // Called after the opening backtick is consumed.
    // Advances past the closing backtick.
    while (pos_ < size_) {
        if (data_[pos_] == '\\') {
            adv();
            if (pos_ < size_) {
                if (data_[pos_] == '\n') advanceLine();
                adv();
            }
        } else if (data_[pos_] == '`') {
            adv();
            return;
        } else if (data_[pos_] == '$' && pos_ + 1 < size_ && data_[pos_ + 1] == '{') {
            adv(); adv(); // skip ${
            skipTemplateExpression();
        } else {
            if (data_[pos_] == '\n') advanceLine();
            adv();
        }
    }
}

void Scanner::skipLineComment() {
    // Advances past the newline (or to end of source).
    while (pos_ < size_ && data_[pos_] != '\n') adv();
}

void Scanner::skipBlockComment() {
    // Called after the opening /* is consumed.
    // Advances past the closing */.
    while (pos_ < size_) {
        if (data_[pos_] == '*' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
            adv(); adv();
            return;
        }
        if (data_[pos_] == '\n') advanceLine();
        adv();
    }
}

void Scanner::skipRegexLiteral() {
    // Called after the opening / is consumed.
    // Advances past the closing / and any flags.
    while (pos_ < size_) {
        if (data_[pos_] == '\\') {
            adv();
            if (pos_ < size_) adv();
        } else if (data_[pos_] == '/') {
            adv();
            break;
        } else if (data_[pos_] == '[') {
            adv();
            while (pos_ < size_ && data_[pos_] != ']') {
                if (data_[pos_] == '\\') { adv(); if (pos_ < size_) adv(); }
                else adv();
            }
            if (pos_ < size_) adv(); // skip ]
        } else {
            if (data_[pos_] == '\n') advanceLine();
            adv();
        }
    }
    // Skip regex flags
    while (pos_ < size_ && std::isalpha(static_cast<unsigned char>(data_[pos_]))) adv();
}

bool Scanner::isRegexPosition() const {
    if (lastSignificantChar_ == 0) return true;
    return lastSignificantChar_ == '\n' ||
           isRegexPrefixPunctuator(lastSignificantChar_) ||
           lastWasRegexKeyword_ || lastWasOperator_;
}

// ── Main scan ────────────────────────────────────────────────────────────────

bool Scanner::scan(const char* data, std::size_t size, CCErrorList* errors) {
    data_ = data;
    size_ = size;
    pos_ = 0;
    line_ = 1;
    col_ = 1;
    errors_ = errors;
    segments_.clear();
    std::size_t errorCount = errors_ ? errors_->size() : 0;
    lastSignificantChar_ = 0;
    lastWasOperator_ = false;
    lastWasRegexKeyword_ = false;
    lastIdentifier_.clear();

    std::size_t segmentStart = 0;
    State state = State::Default;
    std::size_t ccBlockStart = 0;

    while (pos_ < size_) {
        char c = data_[pos_];

        switch (state) {

        // ── Default state ────────────────────────────────────────────────
        case State::Default: {
            // Single-quote string
            if (c == '\'') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipQuotedString('\'');
                continue;
            }
            // Double-quote string
            if (c == '"') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipQuotedString('"');
                continue;
            }
            // Template string
            if (c == '`') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipTemplateString();
                continue;
            }

            // Line comment
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                // Check for //@cc_on (must be immediately after //, no spaces)
                std::size_t lookPos = pos_ + 2;
                if (lookPos + 6 <= size_ &&
                    std::memcmp(data_ + lookPos, "@cc_on", 6) == 0 &&
                    (lookPos + 6 >= size_ ||
                     !(std::isalnum(static_cast<unsigned char>(data_[lookPos + 6])) ||
                       data_[lookPos + 6] == '_' || data_[lookPos + 6] == '$')))
                {
                    if (pos_ > segmentStart) {
                        emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                    }
                    // Find end of line
                    std::size_t lineEnd = pos_;
                    while (lineEnd < size_ && data_[lineEnd] != '\n') ++lineEnd;
                    if (lineEnd < size_ && data_[lineEnd] == '\n') ++lineEnd;
                    emitSegment(SegmentType::CCOnLine, pos_, lineEnd);
                    pos_ = lineEnd;
                    col_ = 1;
                    advanceLine();
                    segmentStart = pos_;

                    // Now scan the rest of the source for @ directives.
                    // Everything between directives is normal JS.
                    // We need to track strings/comments to avoid false matches.
                    while (pos_ < size_) {
                        c = data_[pos_];

                        // Skip strings
                        if (c == '\'' || c == '"') {
                            adv();
                            skipQuotedString(c);
                            continue;
                        }
                        // Skip template strings
                        if (c == '`') {
                            adv();
                            skipTemplateString();
                            continue;
                        }
                        // Skip line comments
                        if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                            skipLineComment();
                            continue;
                        }
                        // Skip block comments
                        if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                            adv(); adv();
                            skipBlockComment();
                            continue;
                        }
                        // Skip regex literals
                        if (c == '/') {
                            // Determine if this is a regex or division operator.
                            // After an identifier that is NOT a regex-enabling keyword
                            // (return, typeof, delete, void, throw, new, in, instanceof,
                            // case, yield), or after ) or ], it's division.
                            bool isRegex = true;
                            if (pos_ > 0) {
                                char prev = data_[pos_ - 1];
                                if (isJSIdentifierChar(prev) || prev == ')' || prev == ']') {
                                    // Check if the preceding identifier is a regex keyword
                                    std::size_t identEnd = pos_;
                                    std::size_t identStart = identEnd;
                                    while (identStart > 0 && isJSIdentifierChar(data_[identStart - 1])) {
                                        --identStart;
                                    }
                                    if (identStart < identEnd) {
                                        std::size_t len = identEnd - identStart;
                                        const char* p = data_ + identStart;
                                        isRegex = isRegexEnablingKeyword(p, len);
                                    } else {
                                        // prev is ) or ] — division
                                        isRegex = false;
                                    }
                                }
                            }
                            if (isRegex) {
                                adv(); // skip /
                                skipRegexLiteral();
                                continue;
                            }
                        }

                        // Check for @if, @elif, @else, @end, @set
                        if (c == '@') {
                            CCDirective directive = matchCCDirective(data_, size_, pos_);
                            bool isDirective = directive != CCDirective::None &&
                                               directive != CCDirective::CCOn;

                            if (isDirective) {
                                // Emit any preceding normal JS
                                if (pos_ > segmentStart) {
                                    emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                                }
                                // Conditional compilation remains active after
                                // //@cc_on, so Tokenizer processes the source from
                                // the first real directive through end of input.
                                emitSegment(SegmentType::CCBlock, pos_, size_);
                                pos_ = size_;
                                segmentStart = pos_;
                                goto done_scan;
                            }
                        }

                        if (c == '\n') advanceLine();
                        adv();
                    }

                    // If we get here, no directives were found after //@cc_on
                    // The rest is just normal JS with CC active but no directives.
                    // Emit as normal JS.
                    if (pos_ > segmentStart) {
                        emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                        segmentStart = pos_;
                    }
                    goto done_scan;
                }
                // Regular line comment — skip to end
                skipLineComment();
                continue;
            }

            // Block comment
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                // Check for /*@cc_on or /*@directive (must be immediately after /*, no spaces)
                std::size_t lookPos = pos_ + 2;
                bool isCCBlock = false;

                isCCBlock = matchCCDirective(data_, size_, lookPos) != CCDirective::None;

                if (isCCBlock) {
                    if (pos_ > segmentStart) {
                        emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                    }
                    ccBlockStart = pos_;
                    state = State::CCBlock;
                    adv(); adv(); // skip /*
                    continue;
                }
                // Regular block comment
                adv(); adv(); // skip /*
                skipBlockComment();
                continue;
            }

            // Regex literal
            if (c == '/' && isRegexPosition()) {
                lastSignificantChar_ = '/';
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv(); // skip /
                skipRegexLiteral();
                continue;
            }

            // Track significant characters
            if (c == ' ' || c == '\t' || c == '\r') {
                // skip
            } else if (c == '\n') {
                advanceLine();
                lastSignificantChar_ = '\n';
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                // Accumulate identifier characters
                lastIdentifier_.push_back(c);
                lastWasOperator_ = false;
                lastSignificantChar_ = c;
            } else {
                // Non-identifier: check if accumulated identifier is a
                // JS keyword that enables regex position (return, typeof, etc.)
                if (!lastIdentifier_.empty()) {
                    lastWasRegexKeyword_ = isRegexEnablingKeyword(
                        lastIdentifier_.data(), lastIdentifier_.size());
                    lastIdentifier_.clear();
                } else {
                    lastWasRegexKeyword_ = false;
                }
                lastWasOperator_ = isRegexPrefixPunctuator(c);
                lastSignificantChar_ = c;
            }

            adv();
            continue;
        }

        // ── CC Block (inside /*@cc_on ... @*/) ───────────────────────────
        case State::CCBlock: {
            if (c == '\'' || c == '"') {
                adv();
                skipQuotedString(c);
                continue;
            }
            if (c == '`') {
                adv();
                skipTemplateString();
                continue;
            }
            // Closing @*/
            if (c == '@' && pos_ + 2 < size_ && data_[pos_ + 1] == '*' && data_[pos_ + 2] == '/') {
                adv(); adv(); adv(); // skip @*/
                emitSegment(SegmentType::CCBlock, ccBlockStart, pos_);
                segmentStart = pos_;
                state = State::Default;
                continue;
            }
            // Nested block comment
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                state = State::CCBlockBlockComment;
                adv(); adv();
                continue;
            }
            // Line comment inside CC
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                state = State::CCBlockLineComment;
                adv(); adv();
                continue;
            }
            if (c == '\n') advanceLine();
            adv();
            continue;
        }

        case State::CCBlockLineComment: {
            if (c == '\n') {
                state = State::CCBlock;
                advanceLine();
            }
            adv();
            continue;
        }

        case State::CCBlockBlockComment: {
            if (c == '*' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                state = State::CCBlock;
                adv(); adv();
                continue;
            }
            if (c == '\n') advanceLine();
            adv();
            continue;
        }

        // Other states handled inline above (strings, comments, regex)
        default:
            adv();
            continue;

        } // switch
    } // while

done_scan:

    // Handle unterminated states
    switch (state) {
        case State::CCBlock:
        case State::CCBlockBlockComment:
        case State::CCBlockLineComment:
            addError(line_, col_, "Unterminated CC block (missing @*/)");
            emitSegment(SegmentType::CCBlock, ccBlockStart, size_);
            segmentStart = size_;
            break;
        default:
            break;
    }

    // Emit any remaining normal JS
    if (segmentStart < size_) {
        emitSegment(SegmentType::NormalJS, segmentStart, size_);
    }

    return !errors_ || errors_->size() == errorCount;
}

} // namespace jscriptcc

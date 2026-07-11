#include "detail/Scanner.h"
#include "detail/CCLexicalRules.h"
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
            } while (pos_ < size_ && detail::isJSIdentifierChar(data_[pos_]));
            StringSlice word(data_ + start, data_ + pos_);
            regexAllowed = detail::isRegexEnablingKeyword(word.data(), word.size());
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
                } else if (detail::isRegexPrefixPunctuator(c)) {
                    regexAllowed = true;
                }
            }
            adv();
        }
    }
}

void Scanner::skipTemplateString() {
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
    while (pos_ < size_ && data_[pos_] != '\n') adv();
}

void Scanner::skipBlockComment() {
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
    while (pos_ < size_ && std::isalpha(static_cast<unsigned char>(data_[pos_]))) adv();
}

bool Scanner::isRegexPosition() const {
    if (lastSignificantChar_ == 0) return true;
    return lastSignificantChar_ == '\n' ||
           detail::isRegexPrefixPunctuator(lastSignificantChar_) ||
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
            // JavaScript string and template literals.
            if (c == '\'') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipQuotedString('\'');
                continue;
            }
            if (c == '"') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipQuotedString('"');
                continue;
            }
            if (c == '`') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipTemplateString();
                continue;
            }

            // Line comments and the //@cc_on activation directive.
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
                    std::size_t lineEnd = pos_;
                    while (lineEnd < size_ && data_[lineEnd] != '\n') ++lineEnd;
                    if (lineEnd < size_ && data_[lineEnd] == '\n') ++lineEnd;
                    emitSegment(SegmentType::CCOnLine, pos_, lineEnd);
                    pos_ = lineEnd;
                    col_ = 1;
                    advanceLine();
                    segmentStart = pos_;

                    // Search for directives while ignoring JS literals and comments.
                    while (pos_ < size_) {
                        c = data_[pos_];

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
                        if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                            skipLineComment();
                            continue;
                        }
                        if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                            adv(); adv();
                            skipBlockComment();
                            continue;
                        }
                        if (c == '/') {
                            // A slash immediately after a non-keyword identifier, ')' or ']' is division.
                            bool isRegex = true;
                            if (pos_ > 0) {
                                char prev = data_[pos_ - 1];
                                if (detail::isJSIdentifierChar(prev) || prev == ')' || prev == ']') {
                                    std::size_t identEnd = pos_;
                                    std::size_t identStart = identEnd;
                                    while (identStart > 0 && detail::isJSIdentifierChar(data_[identStart - 1])) {
                                        --identStart;
                                    }
                                    if (identStart < identEnd) {
                                        std::size_t len = identEnd - identStart;
                                        const char* p = data_ + identStart;
                                        isRegex = detail::isRegexEnablingKeyword(p, len);
                                    } else {
                                        isRegex = false;
                                    }
                                }
                            }
                            if (isRegex) {
                                adv();
                                skipRegexLiteral();
                                continue;
                            }
                        }

                        if (c == '@') {
                            detail::CCDirective directive = detail::matchCCDirective(data_, size_, pos_);
                            bool isDirective = directive != detail::CCDirective::None &&
                                               directive != detail::CCDirective::CCOn;

                            if (isDirective) {
                                if (pos_ > segmentStart) {
                                    emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                                }
                                // //@cc_on keeps CC active through the end of the source.
                                emitSegment(SegmentType::CCBlock, pos_, size_);
                                pos_ = size_;
                                segmentStart = pos_;
                                goto done_scan;
                            }
                        }

                        if (c == '\n') advanceLine();
                        adv();
                    }

                    if (pos_ > segmentStart) {
                        emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                        segmentStart = pos_;
                    }
                    goto done_scan;
                }
                skipLineComment();
                continue;
            }

            // Block comments and embedded CC blocks.
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                // Check for /*@cc_on or /*@directive (must be immediately after /*, no spaces)
                std::size_t lookPos = pos_ + 2;
                bool isCCBlock = false;

                isCCBlock = detail::matchCCDirective(data_, size_, lookPos) != detail::CCDirective::None;

                if (isCCBlock) {
                    if (pos_ > segmentStart) {
                        emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                    }
                    ccBlockStart = pos_;
                    state = State::CCBlock;
                    adv(); adv();
                    continue;
                }
                adv(); adv();
                skipBlockComment();
                continue;
            }

            // Regex literals.
            if (c == '/' && isRegexPosition()) {
                lastSignificantChar_ = '/';
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
                adv();
                skipRegexLiteral();
                continue;
            }

            // Track significant characters
            if (c == ' ' || c == '\t' || c == '\r') {
                // Ignore horizontal whitespace.
            } else if (c == '\n') {
                advanceLine();
                lastSignificantChar_ = '\n';
                lastWasOperator_ = false;
                lastWasRegexKeyword_ = false;
                lastIdentifier_.clear();
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                lastIdentifier_.push_back(c);
                lastWasOperator_ = false;
                lastSignificantChar_ = c;
            } else {
                // Keywords such as return allow a regex literal to follow.
                if (!lastIdentifier_.empty()) {
                    lastWasRegexKeyword_ = detail::isRegexEnablingKeyword(
                        lastIdentifier_.data(), lastIdentifier_.size());
                    lastIdentifier_.clear();
                } else {
                    lastWasRegexKeyword_ = false;
                }
                lastWasOperator_ = detail::isRegexPrefixPunctuator(c);
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
            // End of the current CC block.
            if (c == '@' && pos_ + 2 < size_ && data_[pos_ + 1] == '*' && data_[pos_ + 2] == '/') {
                adv(); adv(); adv();
                emitSegment(SegmentType::CCBlock, ccBlockStart, pos_);
                segmentStart = pos_;
                state = State::Default;
                continue;
            }
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                state = State::CCBlockBlockComment;
                adv(); adv();
                continue;
            }
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

    if (segmentStart < size_) {
        emitSegment(SegmentType::NormalJS, segmentStart, size_);
    }

    return !errors_ || errors_->size() == errorCount;
}

} // namespace jscriptcc

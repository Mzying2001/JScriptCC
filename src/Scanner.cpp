#include "jscriptcc/Scanner.h"
#include <cctype>

namespace jscriptcc {

// ── Helpers ──────────────────────────────────────────────────────────────────

char Scanner::peek(std::size_t offset) const {
    std::size_t p = pos_ + offset;
    return (p < size_) ? data_[p] : '\0';
}

char Scanner::peekNext(std::size_t offset) const {
    return peek(offset + 1);
}

bool Scanner::matchAhead(const char* s, std::size_t offset) const {
    std::size_t slen = std::strlen(s);
    std::size_t p = pos_ + offset;
    if (p + slen > size_) return false;
    return std::memcmp(data_ + p, s, slen) == 0;
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

bool Scanner::isRegexPosition() const {
    if (lastSignificantChar_ == 0) return true;
    switch (lastSignificantChar_) {
        case '(': case '[': case '{': case ',': case ';':
        case '!': case '&': case '|': case '^': case '~':
        case '+': case '-': case '=': case '?': case ':':
        case '\n': case '>':
            return true;
        default:
            return lastWasKeyword_ || lastWasOperator_;
    }
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
    lastSignificantChar_ = 0;
    lastWasOperator_ = false;
    lastWasKeyword_ = false;
    templateBraceDepth_ = 0;

    std::size_t segmentStart = 0;
    State state = State::Default;
    std::size_t ccBlockStart = 0;

    // Advance helper that also updates position
    auto adv = [&]() {
        ++pos_;
        ++col_;
    };

    while (pos_ < size_) {
        char c = data_[pos_];

        switch (state) {

        // ── Default state ────────────────────────────────────────────────
        case State::Default: {
            // Single-quote string
            if (c == '\'') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasKeyword_ = false;
                adv();
                while (pos_ < size_) {
                    if (data_[pos_] == '\\') {
                        adv();
                        if (pos_ < size_) {
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                    } else if (data_[pos_] == '\'') {
                        adv();
                        break;
                    } else {
                        if (data_[pos_] == '\n') advanceLine();
                        adv();
                    }
                }
                continue;
            }
            // Double-quote string
            if (c == '"') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasKeyword_ = false;
                adv();
                while (pos_ < size_) {
                    if (data_[pos_] == '\\') {
                        adv();
                        if (pos_ < size_) {
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                    } else if (data_[pos_] == '"') {
                        adv();
                        break;
                    } else {
                        if (data_[pos_] == '\n') advanceLine();
                        adv();
                    }
                }
                continue;
            }
            // Template string
            if (c == '`') {
                lastSignificantChar_ = c;
                lastWasOperator_ = false;
                lastWasKeyword_ = false;
                adv();
                while (pos_ < size_) {
                    if (data_[pos_] == '\\') {
                        adv();
                        if (pos_ < size_) {
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                    } else if (data_[pos_] == '`') {
                        adv();
                        break;
                    } else if (data_[pos_] == '$' && pos_ + 1 < size_ && data_[pos_ + 1] == '{') {
                        // Template expression
                        int depth = 1;
                        adv(); adv(); // skip ${
                        while (pos_ < size_ && depth > 0) {
                            if (data_[pos_] == '{') ++depth;
                            else if (data_[pos_] == '}') { --depth; if (depth == 0) { adv(); break; } }
                            // Handle strings inside template expression
                            if (data_[pos_] == '\'' || data_[pos_] == '"') {
                                char q = data_[pos_];
                                adv();
                                while (pos_ < size_ && data_[pos_] != q) {
                                    if (data_[pos_] == '\\') { adv(); if (pos_ < size_) adv(); }
                                    else { if (data_[pos_] == '\n') advanceLine(); adv(); }
                                }
                                if (pos_ < size_) adv();
                                continue;
                            }
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                        continue;
                    } else {
                        if (data_[pos_] == '\n') advanceLine();
                        adv();
                    }
                }
                continue;
            }

            // Line comment
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                // Check for //@cc_on (must be immediately after //, no spaces)
                std::size_t lookPos = pos_ + 2;
                if (lookPos + 6 <= size_ &&
                    std::memcmp(data_ + lookPos, "@cc_on", 6) == 0 &&
                    (lookPos + 6 >= size_ || !std::isalnum(static_cast<unsigned char>(data_[lookPos + 6]))))
                {
                    // //@cc_on found
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
                    std::size_t ccRestStart = pos_;
                    bool hadDirective = false;

                    while (pos_ < size_) {
                        c = data_[pos_];

                        // Skip strings
                        if (c == '\'' || c == '"') {
                            char q = c;
                            adv();
                            while (pos_ < size_) {
                                if (data_[pos_] == '\\') {
                                    adv();
                                    if (pos_ < size_) {
                                        if (data_[pos_] == '\n') advanceLine();
                                        adv();
                                    }
                                } else if (data_[pos_] == q) {
                                    adv();
                                    break;
                                } else {
                                    if (data_[pos_] == '\n') advanceLine();
                                    adv();
                                }
                            }
                            continue;
                        }
                        // Skip template strings
                        if (c == '`') {
                            adv();
                            while (pos_ < size_) {
                                if (data_[pos_] == '\\') {
                                    adv();
                                    if (pos_ < size_) {
                                        if (data_[pos_] == '\n') advanceLine();
                                        adv();
                                    }
                                } else if (data_[pos_] == '`') {
                                    adv();
                                    break;
                                } else if (data_[pos_] == '$' && pos_ + 1 < size_ && data_[pos_ + 1] == '{') {
                                    int depth = 1;
                                    adv(); adv();
                                    while (pos_ < size_ && depth > 0) {
                                        if (data_[pos_] == '{') ++depth;
                                        else if (data_[pos_] == '}') { --depth; if (depth == 0) { adv(); break; } }
                                        if (data_[pos_] == '\'' || data_[pos_] == '"') {
                                            char q2 = data_[pos_];
                                            adv();
                                            while (pos_ < size_ && data_[pos_] != q2) {
                                                if (data_[pos_] == '\\') { adv(); if (pos_ < size_) adv(); }
                                                else { if (data_[pos_] == '\n') advanceLine(); adv(); }
                                            }
                                            if (pos_ < size_) adv();
                                            continue;
                                        }
                                        if (data_[pos_] == '\n') advanceLine();
                                        adv();
                                    }
                                    continue;
                                } else {
                                    if (data_[pos_] == '\n') advanceLine();
                                    adv();
                                }
                            }
                            continue;
                        }
                        // Skip line comments
                        if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                            while (pos_ < size_ && data_[pos_] != '\n') adv();
                            continue;
                        }
                        // Skip block comments
                        if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                            adv(); adv();
                            while (pos_ < size_) {
                                if (data_[pos_] == '*' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                                    adv(); adv();
                                    break;
                                }
                                if (data_[pos_] == '\n') advanceLine();
                                adv();
                            }
                            continue;
                        }
                        // Skip regex literals
                        if (c == '/') {
                            // Could be regex or division. Use simple heuristic:
                            // if previous non-whitespace is an identifier char or ) or ],
                            // it's division; otherwise regex.
                            bool isRegex = true;
                            if (pos_ > 0) {
                                char prev = data_[pos_ - 1];
                                if (std::isalnum(static_cast<unsigned char>(prev)) || prev == ')' || prev == ']' || prev == '_') {
                                    isRegex = false;
                                }
                            }
                            if (isRegex) {
                                adv(); // skip /
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
                                continue;
                            }
                        }

                        // Check for @if, @elif, @else, @end, @set
                        if (c == '@') {
                            auto isIdentChar = [](char ch) -> bool {
                                return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
                            };

                            bool isDirective = false;
                            const char* directiveName = nullptr;
                            std::size_t directiveLen = 0;

                            // Check each directive
                            struct DirCheck { const char* name; std::size_t len; };
                            DirCheck dirs[] = {
                                {"@if", 3}, {"@elif", 5}, {"@else", 5},
                                {"@end", 4}, {"@set", 4}
                            };
                            for (auto& d : dirs) {
                                if (pos_ + d.len <= size_ &&
                                    std::memcmp(data_ + pos_, d.name, d.len) == 0 &&
                                    (pos_ + d.len >= size_ || !isIdentChar(data_[pos_ + d.len])))
                                {
                                    isDirective = true;
                                    directiveName = d.name;
                                    directiveLen = d.len;
                                    break;
                                }
                            }

                            if (isDirective) {
                                // Emit any preceding normal JS
                                if (pos_ > segmentStart) {
                                    emitSegment(SegmentType::NormalJS, segmentStart, pos_);
                                }
                                // We don't emit the directive here — we let the
                                // rest of the scanner handle it. Actually, for
                                // the //@cc_on case, we need to emit a CC block
                                // that contains this directive and everything
                                // until the next directive or end of source.
                                //
                                // Wait, actually the simplest approach: emit
                                // each @ directive as part of a CC block that
                                // extends until the next @ directive (at the
                                // start of a line) or end of source.
                                //
                                // But that's complex. Simpler: just emit
                                // everything from //@cc_on to end as one CC block.
                                // The tokenizer handles extracting directives.
                                //
                                // But we already skipped strings/comments above,
                                // so we know where the @ directives are. Let me
                                // reconsider.
                                //
                                // Actually the SIMPLEST correct approach:
                                // After //@cc_on, emit the ENTIRE remaining
                                // source as a CCBlock. The tokenizer will
                                // handle it. But the tokenizer needs to be
                                // smart enough to handle strings inside CC blocks.
                                //
                                // Let me just do that. We'll make the tokenizer
                                // handle strings/comments within CC content.
                                if (!hadDirective) {
                                    // First directive after //@cc_on
                                    // Emit everything from //@cc_on end to here as CCBlock
                                    // Wait, we already advanced past strings/comments.
                                    // We can't just emit from ccRestStart to pos_
                                    // because we've been skipping strings.
                                    //
                                    // Actually the right approach: just emit the
                                    // rest of the source as one CCBlock and break.
                                    emitSegment(SegmentType::CCBlock, pos_, size_);
                                    pos_ = size_;
                                    segmentStart = pos_;
                                    goto done_scan;
                                }
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
                while (pos_ < size_ && data_[pos_] != '\n') adv();
                continue;
            }

            // Block comment
            if (c == '/' && pos_ + 1 < size_ && data_[pos_ + 1] == '*') {
                // Check for /*@cc_on (must be immediately after /*, no spaces)
                std::size_t lookPos = pos_ + 2;
                if (lookPos + 6 <= size_ &&
                    std::memcmp(data_ + lookPos, "@cc_on", 6) == 0 &&
                    (lookPos + 6 >= size_ ||
                     data_[lookPos + 6] == ' ' || data_[lookPos + 6] == '\t' ||
                     data_[lookPos + 6] == '\n' || data_[lookPos + 6] == '\r' ||
                     data_[lookPos + 6] == '*'))
                {
                    // /*@cc_on ... @*/
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
                while (pos_ < size_) {
                    if (data_[pos_] == '*' && pos_ + 1 < size_ && data_[pos_ + 1] == '/') {
                        adv(); adv();
                        break;
                    }
                    if (data_[pos_] == '\n') advanceLine();
                    adv();
                }
                continue;
            }

            // Regex literal
            if (c == '/' && isRegexPosition()) {
                lastSignificantChar_ = '/';
                lastWasOperator_ = false;
                lastWasKeyword_ = false;
                adv(); // skip /
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
                        if (pos_ < size_) adv();
                    } else {
                        if (data_[pos_] == '\n') advanceLine();
                        adv();
                    }
                }
                while (pos_ < size_ && std::isalpha(static_cast<unsigned char>(data_[pos_]))) adv();
                continue;
            }

            // Track significant characters
            if (c == ' ' || c == '\t' || c == '\r') {
                // skip
            } else if (c == '\n') {
                advanceLine();
                lastSignificantChar_ = '\n';
                lastWasOperator_ = false;
                lastWasKeyword_ = false;
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                lastWasKeyword_ = true;
                lastWasOperator_ = false;
                lastSignificantChar_ = c;
            } else {
                lastWasKeyword_ = false;
                switch (c) {
                    case '+': case '-': case '*': case '%':
                    case '<': case '>': case '=': case '!':
                    case '&': case '|': case '^': case '~':
                    case '?': case ':':
                        lastWasOperator_ = true;
                        break;
                    default:
                        lastWasOperator_ = false;
                        break;
                }
                lastSignificantChar_ = c;
            }

            adv();
            continue;
        }

        // ── CC Block (inside /*@cc_on ... @*/) ───────────────────────────
        case State::CCBlock: {
            if (c == '\'' || c == '"') {
                char q = c;
                adv();
                while (pos_ < size_) {
                    if (data_[pos_] == '\\') {
                        adv();
                        if (pos_ < size_) {
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                    } else if (data_[pos_] == q) {
                        adv();
                        break;
                    } else {
                        if (data_[pos_] == '\n') advanceLine();
                        adv();
                    }
                }
                continue;
            }
            if (c == '`') {
                adv();
                while (pos_ < size_) {
                    if (data_[pos_] == '\\') {
                        adv();
                        if (pos_ < size_) {
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                    } else if (data_[pos_] == '`') {
                        adv();
                        break;
                    } else if (data_[pos_] == '$' && pos_ + 1 < size_ && data_[pos_ + 1] == '{') {
                        int depth = 1;
                        adv(); adv();
                        while (pos_ < size_ && depth > 0) {
                            if (data_[pos_] == '{') ++depth;
                            else if (data_[pos_] == '}') { --depth; if (depth == 0) { adv(); break; } }
                            if (data_[pos_] == '\n') advanceLine();
                            adv();
                        }
                        continue;
                    } else {
                        if (data_[pos_] == '\n') advanceLine();
                        adv();
                    }
                }
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

    return true;
}

} // namespace jscriptcc

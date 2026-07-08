#include "jscriptcc/Tokenizer.h"
#include <cctype>
#include <cstring>

namespace jscriptcc {

// ── Helpers ──────────────────────────────────────────────────────────────────

char Tokenizer::peek(std::size_t offset) const {
    std::size_t p = pos_ + offset;
    return (p < size_) ? data_[p] : '\0';
}

char Tokenizer::advance() {
    char c = data_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
    return c;
}

bool Tokenizer::atEnd() const {
    return pos_ >= size_;
}

void Tokenizer::skipSpaces() {
    while (pos_ < size_ && (data_[pos_] == ' ' || data_[pos_] == '\t' || data_[pos_] == '\r')) {
        advance();
    }
}

void Tokenizer::addToken(TokenType type, const char* begin, const char* end) {
    Token tok;
    tok.type = type;
    tok.text = StringSlice(begin, end);
    tok.line = line_;
    tok.column = col_;
    tokens_.push_back(tok);
}

void Tokenizer::addError(int line, int col, const std::string& msg) {
    if (errors_) {
        errors_->push_back(CCError(line, col, msg));
    }
}

// ── Main tokenize ────────────────────────────────────────────────────────────

bool Tokenizer::tokenize(const char* data, std::size_t size, CCErrorList* errors) {
    data_ = data;
    size_ = size;
    pos_ = 0;
    line_ = 1;
    col_ = 1;
    errors_ = errors;
    tokens_.clear();
    textStart_ = 0;
    inText_ = false;

    while (!atEnd()) {
        scanNext();
    }

    // Flush any trailing text
    if (inText_) {
        addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
        inText_ = false;
    }

    addToken(TokenType::END_OF_INPUT, data_ + pos_, data_ + pos_);
    return true;
}

void Tokenizer::scanNext() {
    char c = peek();

    // Newline
    if (c == '\n') {
        if (inText_) {
            addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
            inText_ = false;
        }
        const char* start = data_ + pos_;
        advance();
        addToken(TokenType::NEWLINE, start, data_ + pos_);
        return;
    }

    // Check for @ directives
    if (c == '@') {
        auto isIdentChar = [](char ch) -> bool {
            return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
        };

        // Try to match directives
        struct DirMatch { const char* name; TokenType type; };
        DirMatch directives[] = {
            {"@cc_on", TokenType::CC_ON},
            {"@if",    TokenType::IF},
            {"@elif",  TokenType::ELIF},
            {"@else",  TokenType::ELSE},
            {"@end",   TokenType::END},
            {"@set",   TokenType::SET},
        };

        for (auto& d : directives) {
            std::size_t len = std::strlen(d.name);
            if (pos_ + len <= size_ &&
                std::memcmp(data_ + pos_, d.name, len) == 0 &&
                (pos_ + len >= size_ || !isIdentChar(data_[pos_ + len])))
            {
                // Flush preceding text
                if (inText_) {
                    addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
                    inText_ = false;
                }
                const char* start = data_ + pos_;
                for (std::size_t i = 0; i < len; ++i) advance();
                addToken(d.type, start, data_ + pos_);
                return;
            }
        }

        // @ followed by identifier (variable like @_win32)
        if (pos_ + 1 < size_ && (std::isalpha(static_cast<unsigned char>(data_[pos_ + 1])) || data_[pos_ + 1] == '_')) {
            // Flush preceding text
            if (inText_) {
                addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
                inText_ = false;
            }
            const char* start = data_ + pos_;
            advance(); // skip @
            while (pos_ < size_ && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                advance();
            }
            addToken(TokenType::IDENTIFIER, start, data_ + pos_);
            return;
        }

        // @ followed by something else — treat as text
        if (!inText_) { textStart_ = pos_; inText_ = true; }
        advance();
        return;
    }

    // Skip spaces (not newlines) — they're part of text
    if (c == ' ' || c == '\t' || c == '\r') {
        if (!inText_) { textStart_ = pos_; inText_ = true; }
        advance();
        return;
    }

    // Check for expression tokens: we need to recognize these within
    // @if/@elif conditions. But the tokenizer doesn't know the context.
    // Strategy: always tokenize these, the parser will use them as needed.

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && pos_ + 1 < size_ && std::isdigit(static_cast<unsigned char>(data_[pos_ + 1]))))
    {
        // Flush preceding text
        if (inText_) {
            addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
            inText_ = false;
        }
        const char* start = data_ + pos_;
        if (c == '0' && pos_ + 1 < size_ && (data_[pos_ + 1] == 'x' || data_[pos_ + 1] == 'X')) {
            advance(); advance(); // 0x
            while (pos_ < size_ && std::isxdigit(static_cast<unsigned char>(peek()))) advance();
        } else {
            while (pos_ < size_ && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            if (pos_ < size_ && peek() == '.') {
                advance();
                while (pos_ < size_ && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            }
            if (pos_ < size_ && (peek() == 'e' || peek() == 'E')) {
                advance();
                if (pos_ < size_ && (peek() == '+' || peek() == '-')) advance();
                while (pos_ < size_ && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            }
        }
        addToken(TokenType::NUMBER, start, data_ + pos_);
        return;
    }

    // Identifiers (non-@ prefixed)
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        // Flush preceding text
        if (inText_) {
            addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
            inText_ = false;
        }
        const char* start = data_ + pos_;
        while (pos_ < size_ && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
            advance();
        }
        // Check for true/false keywords
        StringSlice word(start, data_ + pos_);
        if (word == "true" || word == "false") {
            addToken(TokenType::NUMBER, start, data_ + pos_); // treat as number (bool value)
        } else {
            addToken(TokenType::IDENTIFIER, start, data_ + pos_);
        }
        return;
    }

    // Strings
    if (c == '\'' || c == '"') {
        // Flush preceding text
        if (inText_) {
            addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
            inText_ = false;
        }
        char quote = c;
        const char* start = data_ + pos_;
        advance(); // skip opening quote
        while (pos_ < size_ && peek() != quote) {
            if (peek() == '\\') {
                advance();
                if (pos_ < size_) advance();
            } else {
                if (peek() == '\n') { addError(line_, col_, "Unterminated string"); return; }
                advance();
            }
        }
        if (pos_ < size_) advance(); // skip closing quote
        addToken(TokenType::STRING, start, data_ + pos_);
        return;
    }

    // Operators and punctuation
    // Flush preceding text
    if (inText_) {
        addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
        inText_ = false;
    }

    const char* start = data_ + pos_;

    // Two-character operators first
    if (pos_ + 1 < size_) {
        char c1 = c, c2 = data_[pos_ + 1];
        TokenType twoChar = TokenType::TEXT;
        if (c1 == '<' && c2 == '=') twoChar = TokenType::LE;
        else if (c1 == '>' && c2 == '=') twoChar = TokenType::GE;
        else if (c1 == '=' && c2 == '=') twoChar = TokenType::EQ;
        else if (c1 == '!' && c2 == '=') twoChar = TokenType::NE;
        else if (c1 == '&' && c2 == '&') twoChar = TokenType::AND;
        else if (c1 == '|' && c2 == '|') twoChar = TokenType::OR;
        else if (c1 == '<' && c2 == '<') twoChar = TokenType::SHL;
        else if (c1 == '>' && c2 == '>') twoChar = TokenType::SHR;

        if (twoChar != TokenType::TEXT) {
            advance(); advance();
            addToken(twoChar, start, data_ + pos_);
            return;
        }
    }

    // Single-character operators
    TokenType singleChar = TokenType::TEXT;
    switch (c) {
        case '(': singleChar = TokenType::LPAREN; break;
        case ')': singleChar = TokenType::RPAREN; break;
        case ',': singleChar = TokenType::COMMA; break;
        case '!': singleChar = TokenType::NOT; break;
        case '~': singleChar = TokenType::TILDE; break;
        case '+': singleChar = TokenType::PLUS; break;
        case '-': singleChar = TokenType::MINUS; break;
        case '*': singleChar = TokenType::STAR; break;
        case '/': singleChar = TokenType::SLASH; break;
        case '%': singleChar = TokenType::PERCENT; break;
        case '<': singleChar = TokenType::LT; break;
        case '>': singleChar = TokenType::GT; break;
        case '&': singleChar = TokenType::BITAND; break;
        case '|': singleChar = TokenType::BITOR; break;
        case '^': singleChar = TokenType::BITXOR; break;
        case '=': singleChar = TokenType::ASSIGN; break;
        case '?': singleChar = TokenType::QUESTION; break;
        case ':': singleChar = TokenType::COLON; break;
        default: break;
    }

    if (singleChar != TokenType::TEXT) {
        advance();
        addToken(singleChar, start, data_ + pos_);
        return;
    }

    // Unknown character — treat as text
    if (!inText_) { textStart_ = pos_; inText_ = true; }
    advance();
}

} // namespace jscriptcc

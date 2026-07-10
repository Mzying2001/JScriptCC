#include "jscriptcc/Tokenizer.h"
#include "CCSyntax.h"
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

void Tokenizer::beginText() {
    if (!inText_) {
        textStart_ = pos_;
        inText_ = true;
    }
}

void Tokenizer::scanLineComment() {
    beginText();
    while (!atEnd() && peek() != '\n') advance();
}

void Tokenizer::scanBlockComment() {
    beginText();
    advance();
    advance();
    while (!atEnd()) {
        if (peek() == '*' && peek(1) == '/') {
            advance();
            advance();
            return;
        }
        advance();
    }
}

void Tokenizer::scanTemplateExpression() {
    int depth = 1;
    while (!atEnd() && depth > 0) {
        char c = peek();
        if (c == '\'' || c == '"') {
            char quote = c;
            advance();
            while (!atEnd()) {
                if (peek() == '\\') {
                    advance();
                    if (!atEnd()) advance();
                } else if (peek() == quote) {
                    advance();
                    break;
                } else {
                    advance();
                }
            }
        } else if (c == '`') {
            scanTemplateString();
        } else if (c == '/' && peek(1) == '/') {
            scanLineComment();
        } else if (c == '/' && peek(1) == '*') {
            scanBlockComment();
        } else if (c == '{') {
            ++depth;
            advance();
        } else if (c == '}') {
            --depth;
            advance();
        } else {
            advance();
        }
    }
}

void Tokenizer::scanTemplateString() {
    beginText();
    advance();
    while (!atEnd()) {
        if (peek() == '\\') {
            advance();
            if (!atEnd()) advance();
        } else if (peek() == '`') {
            advance();
            return;
        } else if (peek() == '$' && peek(1) == '{') {
            advance();
            advance();
            scanTemplateExpression();
        } else {
            advance();
        }
    }
}

bool Tokenizer::isRegexPosition() const {
    std::size_t p = pos_;
    while (p > 0 && (data_[p - 1] == ' ' || data_[p - 1] == '\t' || data_[p - 1] == '\r')) --p;
    if (p == 0 || data_[p - 1] == '\n') return true;

    char previous = data_[p - 1];
    if (isRegexPrefixPunctuator(previous)) return true;

    if (isJSIdentifierChar(previous)) {
        std::size_t end = p;
        while (p > 0 && isJSIdentifierChar(data_[p - 1])) {
            --p;
        }
        return isRegexEnablingKeyword(data_ + p, end - p);
    }

    return false;
}

void Tokenizer::scanRegexLiteral() {
    beginText();
    advance();
    bool inCharacterClass = false;
    while (!atEnd()) {
        char c = peek();
        if (c == '\\') {
            advance();
            if (!atEnd()) advance();
        } else if (c == '[') {
            inCharacterClass = true;
            advance();
        } else if (c == ']' && inCharacterClass) {
            inCharacterClass = false;
            advance();
        } else if (c == '/' && !inCharacterClass) {
            advance();
            while (!atEnd() && std::isalpha(static_cast<unsigned char>(peek()))) advance();
            return;
        } else if (c == '\n' || c == '\r') {
            return;
        } else {
            advance();
        }
    }
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
    std::size_t beginPos = static_cast<std::size_t>(begin - data_);
    while (locationPos_ < beginPos) {
        if (data_[locationPos_] == '\n') {
            ++locationLine_;
            locationColumn_ = 1;
        } else {
            ++locationColumn_;
        }
        ++locationPos_;
    }
    tok.line = locationLine_;
    tok.column = locationColumn_;
    tokens_.push_back(tok);
}

void Tokenizer::addError(int line, int col, const std::string& msg) {
    if (errors_) {
        errors_->push_back(CCError(line, col, msg));
    }
}

// ── Main tokenize ────────────────────────────────────────────────────────────

bool Tokenizer::tokenize(
    const char* data,
    std::size_t size,
    CCErrorList* errors,
    int initialLine,
    int initialColumn)
{
    data_ = data;
    size_ = size;
    pos_ = 0;
    line_ = initialLine;
    col_ = initialColumn;
    locationPos_ = 0;
    locationLine_ = initialLine;
    locationColumn_ = initialColumn;
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

    if (c == '/' && peek(1) == '/') {
        scanLineComment();
        return;
    }

    if (c == '/' && peek(1) == '*') {
        scanBlockComment();
        return;
    }

    if (c == '`') {
        scanTemplateString();
        return;
    }

    if (c == '/' && isRegexPosition()) {
        scanRegexLiteral();
        return;
    }

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
        CCDirective directive = matchCCDirective(data_, size_, pos_);
        if (directive != CCDirective::None) {
            TokenType type = TokenType::END_OF_INPUT;
            switch (directive) {
                case CCDirective::CCOn: type = TokenType::CC_ON; break;
                case CCDirective::If:   type = TokenType::IF; break;
                case CCDirective::Elif: type = TokenType::ELIF; break;
                case CCDirective::Else: type = TokenType::ELSE; break;
                case CCDirective::End:  type = TokenType::END; break;
                case CCDirective::Set:  type = TokenType::SET; break;
                case CCDirective::None: break;
            }

            if (inText_) {
                addToken(TokenType::TEXT, data_ + textStart_, data_ + pos_);
                inText_ = false;
            }
            const char* start = data_ + pos_;
            while (pos_ < size_ && isJSIdentifierChar(peek(1))) {
                advance();
            }
            advance();
            addToken(type, start, data_ + pos_);
            return;
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
        beginText();
        advance();
        return;
    }

    // Skip spaces (not newlines) — they're part of text
    if (c == ' ' || c == '\t' || c == '\r') {
        beginText();
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
    beginText();
    advance();
}

} // namespace jscriptcc

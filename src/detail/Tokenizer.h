#pragma once
#include <string>
#include <vector>
#include "jscriptcc/CCError.h"
#include "StringSlice.h"

namespace jscriptcc {

/// Token types within a Conditional Compilation block.
enum class TokenType {
    // CC directives
    CC_ON,        // @cc_on
    IF,           // @if
    ELIF,         // @elif
    ELSE,         // @else
    END,          // @end
    SET,          // @set

    // Literals
    IDENTIFIER,   // @_name or name
    NUMBER,       // 123, 0x1F, 1.5
    STRING,       // "..." or '...'

    // Operators
    LPAREN,       // (
    RPAREN,       // )
    COMMA,        // ,
    NOT,          // !
    TILDE,        // ~
    PLUS,         // +
    MINUS,        // -
    STAR,         // *
    SLASH,        // /
    PERCENT,      // %
    LT,           // <
    LE,           // <=
    GT,           // >
    GE,           // >=
    EQ,           // ==
    NE,           // !=
    AND,          // &&
    OR,           // ||
    BITAND,       // &
    BITOR,        // |
    BITXOR,       // ^
    SHL,          // <<
    SHR,          // >>
    ASSIGN,       // =
    QUESTION,     // ?
    COLON,        // :

    // Structural
    NEWLINE,      // \n
    TEXT,         // Non-directive text (preserved verbatim)

    END_OF_INPUT
};

struct Token {
    TokenType type;
    StringSlice text;   // Points into the CC block source
    int line;
    int column;
};

/// Tokenizes the content of a CC block into a token stream.
/// Does NOT parse normal JavaScript — only CC directives and expressions.
class Tokenizer {
public:
    bool tokenize(
        const char* data,
        std::size_t size,
        CCErrorList* errors = nullptr,
        int initialLine = 1,
        int initialColumn = 1);

    const std::vector<Token>& tokens() const { return tokens_; }

private:
    void scanNext();
    void scanDirectiveOrIdentifier();
    void scanNumber();
    void scanString(char quote);
    void scanText();     // Collect non-directive text
    void scanLineComment();
    void scanBlockComment();
    void scanTemplateString();
    void scanTemplateExpression();
    void scanRegexLiteral();
    bool isRegexPosition() const;
    void beginText();
    void addToken(TokenType type, const char* begin, const char* end);
    void addError(int line, int col, const std::string& msg);
    char peek(std::size_t offset = 0) const;
    char advance();
    void skipSpaces();
    bool atEnd() const;

    const char* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::size_t locationPos_ = 0;
    int locationLine_ = 1;
    int locationColumn_ = 1;
    std::size_t textStart_ = 0;  // Start of current TEXT run
    bool inText_ = false;

    std::vector<Token> tokens_;
    CCErrorList* errors_ = nullptr;
};

} // namespace jscriptcc

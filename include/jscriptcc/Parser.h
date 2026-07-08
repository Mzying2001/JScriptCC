#pragma once
#include <vector>
#include "AST.h"
#include "Tokenizer.h"
#include "CCError.h"

namespace jscriptcc {

/// Parses a CC token stream into an AST.
/// Only parses Conditional Compilation grammar, never JavaScript.
class Parser {
public:
    std::unique_ptr<BlockNode> parse(const std::vector<Token>& tokens, CCErrorList* errors = nullptr);

private:
    // Parsing methods
    ASTNodePtr parseStatement();
    std::unique_ptr<IfNode> parseIf();
    std::unique_ptr<SetNode> parseSet();
    ASTNodePtr parseText();

    // Expression parsing (Pratt parser)
    ExprNodePtr parseExpression(int minPrecedence = 0);
    ExprNodePtr parsePrimary();
    ExprNodePtr parseUnary();
    int getPrecedence(TokenType type) const;
    bool isRightAssociative(TokenType type) const;
    ExprType tokenToBinaryExprType(TokenType type) const;

    // Helpers
    const Token& current() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool atEnd() const;
    void addError(const Token& tok, const std::string& msg);
    void synchronize();
    void skipWhitespace(); // Skip TEXT and NEWLINE tokens

    const std::vector<Token>* tokens_ = nullptr;
    std::size_t pos_ = 0;
    CCErrorList* errors_ = nullptr;
};

} // namespace jscriptcc

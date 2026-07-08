#include "jscriptcc/Parser.h"
#include <cstring>
#include <cstdlib>

namespace jscriptcc {

// ── Helpers ──────────────────────────────────────────────────────────────────

const Token& Parser::current() const {
    if (pos_ >= tokens_->size()) return tokens_->back();
    return (*tokens_)[pos_];
}

const Token& Parser::advance() {
    const Token& t = current();
    if (pos_ < tokens_->size()) ++pos_;
    return t;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

bool Parser::atEnd() const {
    return current().type == TokenType::END_OF_INPUT;
}

void Parser::addError(const Token& tok, const std::string& msg) {
    if (errors_) {
        errors_->push_back(CCError(tok.line, tok.column, msg));
    }
}

void Parser::skipWhitespace() {
    while (!atEnd() && (current().type == TokenType::TEXT || current().type == TokenType::NEWLINE)) {
        advance();
    }
}

void Parser::synchronize() {
    // Skip tokens until we find a statement boundary
    while (!atEnd()) {
        if (current().type == TokenType::IF ||
            current().type == TokenType::ELIF ||
            current().type == TokenType::ELSE ||
            current().type == TokenType::END ||
            current().type == TokenType::SET ||
            current().type == TokenType::NEWLINE) {
            return;
        }
        advance();
    }
}

// ── Main parse ───────────────────────────────────────────────────────────────

std::unique_ptr<BlockNode> Parser::parse(const std::vector<Token>& tokens, CCErrorList* errors) {
    tokens_ = &tokens;
    pos_ = 0;
    errors_ = errors;

    auto block = std::unique_ptr<BlockNode>(new BlockNode());

    while (!atEnd()) {
        // Skip leading newlines
        if (check(TokenType::NEWLINE)) {
            advance();
            continue;
        }

        try {
            auto stmt = parseStatement();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
            }
        } catch (...) {
            synchronize();
        }
    }

    return block;
}

// ── Statement parsing ────────────────────────────────────────────────────────

ASTNodePtr Parser::parseStatement() {
    if (check(TokenType::IF)) {
        return parseIf();
    }
    if (check(TokenType::SET)) {
        return parseSet();
    }
    if (check(TokenType::ELIF) || check(TokenType::ELSE) || check(TokenType::END)) {
        // These are handled by parseIf, seeing them here means they're orphaned
        addError(current(), "Unexpected " + current().text.toString());
        advance();
        return nullptr;
    }
    return parseText();
}

std::unique_ptr<IfNode> Parser::parseIf() {
    auto ifNode = std::unique_ptr<IfNode>(new IfNode());

    // Parse @if
    advance(); // consume @if
    {
        Branch branch;
        // Condition is optional in JScript CC (bare @if evaluates the expression)
        if (check(TokenType::LPAREN)) {
            advance(); // consume (
            branch.condition = parseExpression();
            skipWhitespace();
            if (!check(TokenType::RPAREN)) {
                addError(current(), "Expected ')' after @if condition");
            } else {
                advance(); // consume )
            }
        } else if (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_INPUT) &&
                   current().type != TokenType::IF && current().type != TokenType::ELIF &&
                   current().type != TokenType::ELSE && current().type != TokenType::END &&
                   current().type != TokenType::SET) {
            // Bare expression without parentheses
            branch.condition = parseExpression();
        } else {
            addError(current(), "@if requires a condition");
        }

        // Parse body (everything until @elif, @else, @end)
        while (!atEnd() && !check(TokenType::ELIF) && !check(TokenType::ELSE) &&
               !check(TokenType::END)) {
            auto stmt = parseStatement();
            if (stmt) branch.body.push_back(std::move(stmt));
        }

        ifNode->branches.push_back(std::move(branch));
    }

    // Parse @elif branches
    while (check(TokenType::ELIF)) {
        advance(); // consume @elif
        Branch branch;
        if (check(TokenType::LPAREN)) {
            advance();
            branch.condition = parseExpression();
            skipWhitespace();
            if (!check(TokenType::RPAREN)) {
                addError(current(), "Expected ')' after @elif condition");
            } else {
                advance();
            }
        } else if (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_INPUT) &&
                   current().type != TokenType::IF && current().type != TokenType::ELIF &&
                   current().type != TokenType::ELSE && current().type != TokenType::END &&
                   current().type != TokenType::SET) {
            branch.condition = parseExpression();
        } else {
            addError(current(), "@elif requires a condition");
        }

        while (!atEnd() && !check(TokenType::ELIF) && !check(TokenType::ELSE) &&
               !check(TokenType::END)) {
            auto stmt = parseStatement();
            if (stmt) branch.body.push_back(std::move(stmt));
        }

        ifNode->branches.push_back(std::move(branch));
    }

    // Parse @else
    if (check(TokenType::ELSE)) {
        advance(); // consume @else
        Branch branch;
        // No condition for @else

        while (!atEnd() && !check(TokenType::END)) {
            auto stmt = parseStatement();
            if (stmt) branch.body.push_back(std::move(stmt));
        }

        ifNode->branches.push_back(std::move(branch));
    }

    // Expect @end
    if (check(TokenType::END)) {
        advance(); // consume @end
    } else {
        addError(current(), "Expected @end to close @if block");
    }

    return ifNode;
}

std::unique_ptr<SetNode> Parser::parseSet() {
    auto setNode = std::unique_ptr<SetNode>(new SetNode());
    advance(); // consume @set
    skipWhitespace();

    // Expect identifier (variable name, may start with @)
    if (check(TokenType::IDENTIFIER)) {
        setNode->varName = current().text;
        advance();
    } else {
        addError(current(), "Expected variable name after @set");
        return setNode;
    }

    skipWhitespace();

    // Expect =
    if (check(TokenType::ASSIGN)) {
        advance();
        skipWhitespace();
    } else {
        addError(current(), "Expected '=' after variable name in @set");
        return setNode;
    }

    // Parse expression
    setNode->value = parseExpression();

    return setNode;
}

ASTNodePtr Parser::parseText() {
    // Collect consecutive text tokens and newlines
    // If we encounter a directive, stop
    auto textNode = std::unique_ptr<TextNode>(new TextNode());
    const char* start = current().text.begin();
    const char* end = start;

    while (!atEnd() &&
           current().type != TokenType::IF &&
           current().type != TokenType::ELIF &&
           current().type != TokenType::ELSE &&
           current().type != TokenType::END &&
           current().type != TokenType::SET &&
           current().type != TokenType::CC_ON) {
        end = current().text.end();
        advance();
    }

    if (end > start) {
        textNode->text = StringSlice(start, end);
        return textNode;
    }

    return nullptr;
}

// ── Expression parsing (Pratt parser) ────────────────────────────────────────

int Parser::getPrecedence(TokenType type) const {
    switch (type) {
        case TokenType::OR:       return 2;
        case TokenType::AND:      return 3;
        case TokenType::BITOR:    return 4;
        case TokenType::BITXOR:   return 5;
        case TokenType::BITAND:   return 6;
        case TokenType::EQ:
        case TokenType::NE:       return 7;
        case TokenType::LT:
        case TokenType::LE:
        case TokenType::GT:
        case TokenType::GE:       return 8;
        case TokenType::SHL:
        case TokenType::SHR:      return 9;
        case TokenType::PLUS:
        case TokenType::MINUS:    return 10;
        case TokenType::STAR:
        case TokenType::SLASH:
        case TokenType::PERCENT:  return 11;
        default:                  return -1; // not a binary operator
    }
}

bool Parser::isRightAssociative(TokenType /*type*/) const {
    // None of the CC operators are right-associative
    return false;
}

ExprType Parser::tokenToBinaryExprType(TokenType type) const {
    switch (type) {
        case TokenType::STAR:     return ExprType::Mul;
        case TokenType::SLASH:    return ExprType::Div;
        case TokenType::PERCENT:  return ExprType::Mod;
        case TokenType::PLUS:     return ExprType::Add;
        case TokenType::MINUS:    return ExprType::Sub;
        case TokenType::SHL:      return ExprType::Shl;
        case TokenType::SHR:      return ExprType::Shr;
        case TokenType::LT:       return ExprType::Lt;
        case TokenType::LE:       return ExprType::Le;
        case TokenType::GT:       return ExprType::Gt;
        case TokenType::GE:       return ExprType::Ge;
        case TokenType::EQ:       return ExprType::Eq;
        case TokenType::NE:       return ExprType::Ne;
        case TokenType::BITAND:   return ExprType::BitAnd;
        case TokenType::BITXOR:   return ExprType::BitXor;
        case TokenType::BITOR:    return ExprType::BitOr;
        case TokenType::AND:      return ExprType::LogAnd;
        case TokenType::OR:       return ExprType::LogOr;
        default:                  return ExprType::NumberLiteral; // shouldn't happen
    }
}

ExprNodePtr Parser::parseExpression(int minPrecedence) {
    ExprNodePtr left = parseUnary();

    while (!atEnd()) {
        skipWhitespace();
        if (atEnd()) break;

        int prec = getPrecedence(current().type);
        if (prec < minPrecedence) break;

        TokenType opType = current().type;
        advance();

        skipWhitespace();

        ExprType binType = tokenToBinaryExprType(opType);
        int nextMinPrec = isRightAssociative(opType) ? prec : prec + 1;
        ExprNodePtr right = parseExpression(nextMinPrec);

        auto node = ExprNode::make(binType);
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }

    return left;
}

ExprNodePtr Parser::parseUnary() {
    skipWhitespace();
    switch (current().type) {
        case TokenType::NOT: {
            advance();
            auto operand = parseUnary();
            auto node = ExprNode::make(ExprType::UnaryNot);
            node->left = std::move(operand);
            return node;
        }
        case TokenType::TILDE: {
            advance();
            auto operand = parseUnary();
            auto node = ExprNode::make(ExprType::UnaryBitNot);
            node->left = std::move(operand);
            return node;
        }
        case TokenType::PLUS: {
            advance();
            auto operand = parseUnary();
            auto node = ExprNode::make(ExprType::UnaryPlus);
            node->left = std::move(operand);
            return node;
        }
        case TokenType::MINUS: {
            advance();
            auto operand = parseUnary();
            auto node = ExprNode::make(ExprType::UnaryMinus);
            node->left = std::move(operand);
            return node;
        }
        default:
            return parsePrimary();
    }
}

ExprNodePtr Parser::parsePrimary() {
    skipWhitespace();
    const Token& tok = current();

    switch (tok.type) {
        case TokenType::LPAREN: {
            advance(); // (
            auto expr = parseExpression();
            if (!check(TokenType::RPAREN)) {
                addError(current(), "Expected ')' in expression");
            } else {
                advance(); // )
            }
            return expr;
        }

        case TokenType::NUMBER: {
            advance();
            auto node = ExprNode::make(ExprType::NumberLiteral);
            node->valueText = tok.text;
            return node;
        }

        case TokenType::STRING: {
            advance();
            auto node = ExprNode::make(ExprType::StringLiteral);
            node->valueText = tok.text;
            return node;
        }

        case TokenType::IDENTIFIER: {
            advance();
            auto node = ExprNode::make(ExprType::Identifier);
            node->valueText = tok.text;
            return node;
        }

        default:
            addError(tok, "Expected expression");
            advance(); // skip the unexpected token
            auto node = ExprNode::make(ExprType::NumberLiteral);
            node->valueText = StringSlice("0", 1);
            return node;
    }
}

} // namespace jscriptcc

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "StringSlice.h"

namespace jscriptcc {

/// AST node types for Conditional Compilation.
enum class NodeType {
    Block,
    If,
    Set,
    Text,
    Expression
};

/// Base AST node.
struct ASTNode {
    virtual ~ASTNode() {}
    virtual NodeType nodeType() const = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

/// Expression node (for conditions in @if/@elif).
enum class ExprType {
    // Literals
    NumberLiteral,
    StringLiteral,
    BoolLiteral,
    Identifier,

    // Unary
    UnaryNot,
    UnaryBitNot,
    UnaryPlus,
    UnaryMinus,

    // Binary
    Mul, Div, Mod,
    Add, Sub,
    Shl, Shr,
    Lt, Le, Gt, Ge,
    Eq, Ne,
    BitAnd, BitXor, BitOr,
    LogAnd, LogOr,

    // Ternary
    Conditional
};

struct ExprNode : public ASTNode {
    ExprType exprType;

    // For literals
    StringSlice valueText;  // Raw text of the value

    // For binary/unary: children
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
    std::unique_ptr<ExprNode> third;  // For ternary

    // For conditional: condition ? trueExpr : falseExpr
    // left = condition, right = trueExpr, third = falseExpr

    NodeType nodeType() const override { return NodeType::Expression; }

    static std::unique_ptr<ExprNode> make(ExprType t) {
        auto n = std::unique_ptr<ExprNode>(new ExprNode());
        n->exprType = t;
        return n;
    }
};

using ExprNodePtr = std::unique_ptr<ExprNode>;

/// A branch in an @if/@elif/@else chain.
struct Branch {
    ExprNodePtr condition;  // null for @else
    std::vector<ASTNodePtr> body;
    StringSlice conditionText;  // Original text of the condition (for debugging)
};

/// @if node with one or more branches (@if, @elif, @else) and @end.
struct IfNode : public ASTNode {
    std::vector<Branch> branches;

    NodeType nodeType() const override { return NodeType::If; }
};

/// @set node: @set varname = expression
struct SetNode : public ASTNode {
    StringSlice varName;
    ExprNodePtr value;
    StringSlice valueText;  // Original expression text

    NodeType nodeType() const override { return NodeType::Set; }
};

/// Raw text node (non-directive content inside a CC block).
struct TextNode : public ASTNode {
    StringSlice text;

    NodeType nodeType() const override { return NodeType::Text; }

    static std::unique_ptr<TextNode> make(const StringSlice& t) {
        auto n = std::unique_ptr<TextNode>(new TextNode());
        n->text = t;
        return n;
    }
};

/// Root block node.
struct BlockNode : public ASTNode {
    std::vector<ASTNodePtr> statements;

    NodeType nodeType() const override { return NodeType::Block; }
};

} // namespace jscriptcc

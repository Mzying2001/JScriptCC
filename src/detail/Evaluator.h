#pragma once
#include "AST.h"
#include "jscriptcc/CCEnvironment.h"
#include "jscriptcc/CCError.h"

namespace jscriptcc {

/// Result of evaluating a CC block.
struct EvalResult {
    std::string outputText;   // The text from the selected branch (or empty)
    bool hasOutput;           // Whether this block produced output
};

/// Evaluates a CC AST against an environment.
/// Determines which @if/@elif/@else branch to keep, and executes @set.
class Evaluator {
public:
    EvalResult evaluate(const BlockNode& block, CCEnvironment& env, CCErrorList* errors = nullptr);

private:
    // Statement evaluation
    void evalStatement(const ASTNode& node, std::string& output);
    void evalIf(const IfNode& node, std::string& output);
    void evalSet(const SetNode& node);
    void evalText(const TextNode& node, std::string& output);

    // Expression evaluation
    CCValue evalExpr(const ExprNode& node);

    CCEnvironment* env_ = nullptr;
    CCErrorList* errors_ = nullptr;
};

} // namespace jscriptcc

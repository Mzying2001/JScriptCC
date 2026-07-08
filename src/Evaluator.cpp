#include "jscriptcc/Evaluator.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <limits>

namespace jscriptcc {

// Safe double-to-int32 conversion matching JScript's ToInt32 semantics.
// Returns 0 for NaN, Infinity, or values outside int range.
static int safeToInt32(double v) {
    if (std::isnan(v) || std::isinf(v) || v == 0.0) return 0;
    // Use fmod to match ToInt32: n modulo 2^32, then reinterpret as signed
    double two32 = 4294967296.0; // 2^32
    double n = std::fmod(v, two32);
    if (n < 0) n += two32;
    // Now n is in [0, 2^32). Convert to int32 via two's complement.
    if (n >= 2147483648.0) return static_cast<int>(n - two32);
    return static_cast<int>(n);
}

// ── Main evaluate ────────────────────────────────────────────────────────────

EvalResult Evaluator::evaluate(const BlockNode& block, CCEnvironment& env, CCErrorList* errors) {
    env_ = &env;
    errors_ = errors;

    EvalResult result;
    result.hasOutput = false;

    for (const auto& stmt : block.statements) {
        evalStatement(*stmt, result.outputText);
    }

    result.hasOutput = !result.outputText.empty();
    return result;
}

// ── Statement evaluation ─────────────────────────────────────────────────────

void Evaluator::evalStatement(const ASTNode& node, std::string& output) {
    switch (node.nodeType()) {
        case NodeType::If:
            evalIf(static_cast<const IfNode&>(node), output);
            break;
        case NodeType::Set:
            evalSet(static_cast<const SetNode&>(node));
            break;
        case NodeType::Text:
            evalText(static_cast<const TextNode&>(node), output);
            break;
        case NodeType::Block: {
            const auto& block = static_cast<const BlockNode&>(node);
            for (const auto& stmt : block.statements) {
                evalStatement(*stmt, output);
            }
            break;
        }
        default:
            break;
    }
}

void Evaluator::evalIf(const IfNode& node, std::string& output) {
    for (const auto& branch : node.branches) {
        if (!branch.condition) {
            // @else branch — always taken if we reach it
            for (const auto& stmt : branch.body) {
                evalStatement(*stmt, output);
            }
            return;
        }

        CCValue condVal = evalExpr(*branch.condition);
        if (condVal.toBool()) {
            for (const auto& stmt : branch.body) {
                evalStatement(*stmt, output);
            }
            return;
        }
    }
    // No branch matched — output nothing
}

void Evaluator::evalSet(const SetNode& node) {
    if (node.value) {
        CCValue val = evalExpr(*node.value);
        env_->set(node.varName.toString(), val);
    }
}

void Evaluator::evalText(const TextNode& node, std::string& output) {
    output.append(node.text.data(), node.text.size());
}

// ── Expression evaluation ────────────────────────────────────────────────────

CCValue Evaluator::evalExpr(const ExprNode& node) {
    switch (node.exprType) {
        // Literals
        case ExprType::NumberLiteral: {
            const std::string& s = node.valueText.toString();
            // Check for true/false
            if (s == "true") return CCValue(true);
            if (s == "false") return CCValue(false);
            // Check for hex
            if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                long v = std::strtol(s.c_str(), nullptr, 16);
                return CCValue(static_cast<double>(v));
            }
            return CCValue(std::strtod(s.c_str(), nullptr));
        }

        case ExprType::StringLiteral: {
            // Remove quotes
            std::string s = node.valueText.toString();
            if (s.size() >= 2 && (s.front() == '\'' || s.front() == '"') && s.front() == s.back()) {
                s = s.substr(1, s.size() - 2);
            }
            return CCValue(s);
        }

        case ExprType::Identifier: {
            std::string name = node.valueText.toString();
            const CCValue* val = env_->find(name);
            if (val) return *val;
            // Undefined variable — return 0 (JScript behavior)
            return CCValue(0.0);
        }

        // Unary operators
        case ExprType::UnaryNot:
            return CCValue(!evalExpr(*node.left).toBool());

        case ExprType::UnaryBitNot: {
            double v = evalExpr(*node.left).toNumber();
            return CCValue(static_cast<double>(~safeToInt32(v)));
        }

        case ExprType::UnaryPlus:
            return CCValue(evalExpr(*node.left).toNumber());

        case ExprType::UnaryMinus:
            return CCValue(-evalExpr(*node.left).toNumber());

        // Binary operators
        case ExprType::Mul:
            return CCValue(evalExpr(*node.left).toNumber() * evalExpr(*node.right).toNumber());

        case ExprType::Div: {
            double r = evalExpr(*node.right).toNumber();
            if (r == 0) {
                // Match JavaScript semantics: 0/0→NaN, n/0→±Infinity
                double l = evalExpr(*node.left).toNumber();
                if (l == 0) return CCValue(std::numeric_limits<double>::quiet_NaN());
                return CCValue(l > 0 ? std::numeric_limits<double>::infinity()
                                     : -std::numeric_limits<double>::infinity());
            }
            return CCValue(evalExpr(*node.left).toNumber() / r);
        }

        case ExprType::Mod: {
            double r = evalExpr(*node.right).toNumber();
            if (r == 0) return CCValue(std::numeric_limits<double>::quiet_NaN());
            return CCValue(std::fmod(evalExpr(*node.left).toNumber(), r));
        }

        case ExprType::Add: {
            CCValue l = evalExpr(*node.left);
            CCValue r = evalExpr(*node.right);
            if (l.isString() || r.isString()) {
                return CCValue(l.toString() + r.toString());
            }
            return CCValue(l.toNumber() + r.toNumber());
        }

        case ExprType::Sub:
            return CCValue(evalExpr(*node.left).toNumber() - evalExpr(*node.right).toNumber());

        case ExprType::Shl: {
            int l = safeToInt32(evalExpr(*node.left).toNumber());
            int r = safeToInt32(evalExpr(*node.right).toNumber()) & 0x1f;
            return CCValue(static_cast<double>(l << r));
        }

        case ExprType::Shr: {
            int l = safeToInt32(evalExpr(*node.left).toNumber());
            int r = safeToInt32(evalExpr(*node.right).toNumber()) & 0x1f;
            return CCValue(static_cast<double>(l >> r));
        }

        case ExprType::Lt:
            return CCValue(evalExpr(*node.left).toNumber() < evalExpr(*node.right).toNumber());

        case ExprType::Le:
            return CCValue(evalExpr(*node.left).toNumber() <= evalExpr(*node.right).toNumber());

        case ExprType::Gt:
            return CCValue(evalExpr(*node.left).toNumber() > evalExpr(*node.right).toNumber());

        case ExprType::Ge:
            return CCValue(evalExpr(*node.left).toNumber() >= evalExpr(*node.right).toNumber());

        case ExprType::Eq: {
            CCValue l = evalExpr(*node.left);
            CCValue r = evalExpr(*node.right);
            if (l.isString() || r.isString()) {
                return CCValue(l.toString() == r.toString());
            }
            return CCValue(l.toNumber() == r.toNumber());
        }

        case ExprType::Ne: {
            CCValue l = evalExpr(*node.left);
            CCValue r = evalExpr(*node.right);
            if (l.isString() || r.isString()) {
                return CCValue(l.toString() != r.toString());
            }
            return CCValue(l.toNumber() != r.toNumber());
        }

        case ExprType::BitAnd:
            return CCValue(static_cast<double>(
                safeToInt32(evalExpr(*node.left).toNumber()) &
                safeToInt32(evalExpr(*node.right).toNumber())));

        case ExprType::BitXor:
            return CCValue(static_cast<double>(
                safeToInt32(evalExpr(*node.left).toNumber()) ^
                safeToInt32(evalExpr(*node.right).toNumber())));

        case ExprType::BitOr:
            return CCValue(static_cast<double>(
                safeToInt32(evalExpr(*node.left).toNumber()) |
                safeToInt32(evalExpr(*node.right).toNumber())));

        case ExprType::LogAnd: {
            CCValue l = evalExpr(*node.left);
            if (!l.toBool()) return l; // short-circuit
            return evalExpr(*node.right);
        }

        case ExprType::LogOr: {
            CCValue l = evalExpr(*node.left);
            if (l.toBool()) return l; // short-circuit
            return evalExpr(*node.right);
        }

        case ExprType::Conditional: {
            // Ternary: condition ? trueExpr : falseExpr
            // Not supported in this implementation (JScript CC doesn't have ternary)
            if (evalExpr(*node.left).toBool()) {
                return evalExpr(*node.right);
            }
            return evalExpr(*node.third);
        }

        default:
            return CCValue(0.0);
    }
}

} // namespace jscriptcc

#include "detail/Evaluator.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace jscriptcc {

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parseHex4(
    const std::string& text,
    std::size_t start,
    std::size_t end,
    unsigned int& value) {
    if (start > end || end - start < 4)
        return false;
    value = 0;
    for (std::size_t i = start; i < start + 4; ++i) {
        int digit = hexValue(text[i]);
        if (digit < 0) return false;
        value = (value << 4) | static_cast<unsigned int>(digit);
    }
    return true;
}

static void appendUtf8(std::string& output, unsigned int codePoint) {
    if (codePoint <= 0x7f) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0x10ffff) {
        output.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}

static std::string decodeStringLiteral(const StringSlice& text) {
    std::string raw = text.toString();
    if (raw.size() < 2 || (raw.front() != '\'' && raw.front() != '"') || raw.back() != raw.front()) {
        return raw;
    }

    std::string result;
    for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
        char c = raw[i];
        if (c != '\\' || i + 1 >= raw.size() - 1) {
            result.push_back(c);
            continue;
        }

        char escaped = raw[++i];
        switch (escaped) {
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'v': result.push_back('\v'); break;
            case '0': result.push_back('\0'); break;
            case '\\': result.push_back('\\'); break;
            case '\'': result.push_back('\''); break;
            case '"': result.push_back('"'); break;
            case '\n': break;
            case '\r':
                if (i + 1 < raw.size() - 1 && raw[i + 1] == '\n') ++i;
                break;
            case 'x': {
                if (i + 2 < raw.size() - 1) {
                    int high = hexValue(raw[i + 1]);
                    int low = hexValue(raw[i + 2]);
                    if (high >= 0 && low >= 0) {
                        result.push_back(static_cast<char>((high << 4) | low));
                        i += 2;
                        break;
                    }
                }
                result.push_back('x');
                break;
            }
            case 'u': {
                const std::size_t contentEnd = raw.size() - 1;
                unsigned int codePoint = 0;
                if (parseHex4(raw, i + 1, contentEnd, codePoint)) {
                    i += 4;
                    if (codePoint >= 0xd800 && codePoint <= 0xdbff &&
                        i + 2 < contentEnd && raw[i + 1] == '\\' && raw[i + 2] == 'u') {
                        unsigned int lowSurrogate = 0;
                        if (parseHex4(raw, i + 3, contentEnd, lowSurrogate) &&
                            lowSurrogate >= 0xdc00 && lowSurrogate <= 0xdfff) {
                            codePoint = 0x10000 +
                                ((codePoint - 0xd800) << 10) +
                                (lowSurrogate - 0xdc00);
                            i += 6;
                        }
                    }
                    appendUtf8(result, codePoint);
                    break;
                }
                result.push_back('u');
                break;
            }
            default:
                result.push_back(escaped);
                break;
        }
    }
    return result;
}

// Safe double-to-int32 conversion matching JScript's ToInt32 semantics.
// Returns 0 for NaN, Infinity, or values outside int range.
static std::int32_t safeToInt32(double v) {
    if (std::isnan(v) || std::isinf(v) || v == 0.0) return 0;
    // Use fmod to match ToInt32: n modulo 2^32, then reinterpret as signed
    double two32 = 4294967296.0; // 2^32
    double n = std::fmod(v, two32);
    if (n < 0) n += two32;
    // Now n is in [0, 2^32). Convert to int32 via two's complement.
    if (n >= 2147483648.0) return static_cast<std::int32_t>(n - two32);
    return static_cast<std::int32_t>(n);
}

static std::int32_t bitsToInt32(std::uint32_t bits) {
    if (bits < 0x80000000u) return static_cast<std::int32_t>(bits);
    return static_cast<std::int32_t>(static_cast<std::int64_t>(bits) - 0x100000000LL);
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
            return CCValue(decodeStringLiteral(node.valueText));
        }

        case ExprType::Identifier: {
            std::string name = node.valueText.toString();
            const CCValue* val = env_->find(name);
            if (val) return *val;
            return CCValue();
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
            std::uint32_t left = static_cast<std::uint32_t>(
                safeToInt32(evalExpr(*node.left).toNumber()));
            std::uint32_t shift = static_cast<std::uint32_t>(
                safeToInt32(evalExpr(*node.right).toNumber())) & 0x1f;
            return CCValue(static_cast<double>(bitsToInt32(left << shift)));
        }

        case ExprType::Shr: {
            std::uint32_t left = static_cast<std::uint32_t>(
                safeToInt32(evalExpr(*node.left).toNumber()));
            std::uint32_t shift = static_cast<std::uint32_t>(
                safeToInt32(evalExpr(*node.right).toNumber())) & 0x1f;
            std::uint32_t shifted = left >> shift;
            if (shift != 0 && (left & 0x80000000u) != 0) {
                shifted |= 0xffffffffu << (32 - shift);
            }
            return CCValue(static_cast<double>(bitsToInt32(shifted)));
        }

        case ExprType::Lt: {
            CCValue left = evalExpr(*node.left);
            CCValue right = evalExpr(*node.right);
            if (left.isString() && right.isString()) return CCValue(left.str() < right.str());
            return CCValue(left.toNumber() < right.toNumber());
        }

        case ExprType::Le: {
            CCValue left = evalExpr(*node.left);
            CCValue right = evalExpr(*node.right);
            if (left.isString() && right.isString()) return CCValue(left.str() <= right.str());
            return CCValue(left.toNumber() <= right.toNumber());
        }

        case ExprType::Gt: {
            CCValue left = evalExpr(*node.left);
            CCValue right = evalExpr(*node.right);
            if (left.isString() && right.isString()) return CCValue(left.str() > right.str());
            return CCValue(left.toNumber() > right.toNumber());
        }

        case ExprType::Ge: {
            CCValue left = evalExpr(*node.left);
            CCValue right = evalExpr(*node.right);
            if (left.isString() && right.isString()) return CCValue(left.str() >= right.str());
            return CCValue(left.toNumber() >= right.toNumber());
        }

        case ExprType::Eq: {
            CCValue l = evalExpr(*node.left);
            CCValue r = evalExpr(*node.right);
            if (l.type() == r.type()) {
                if (l.isString()) return CCValue(l.str() == r.str());
                if (l.isBool()) return CCValue(l.toBool() == r.toBool());
                if (l.isUndefined()) return CCValue(true);
                return CCValue(l.toNumber() == r.toNumber());
            }
            if (l.isBool()) l = CCValue(l.toNumber());
            if (r.isBool()) r = CCValue(r.toNumber());
            if ((l.isString() && r.isNumber()) || (l.isNumber() && r.isString())) {
                return CCValue(l.toNumber() == r.toNumber());
            }
            return CCValue(l.toNumber() == r.toNumber());
        }

        case ExprType::Ne: {
            CCValue l = evalExpr(*node.left);
            CCValue r = evalExpr(*node.right);
            if (l.type() == r.type()) {
                if (l.isString()) return CCValue(l.str() != r.str());
                if (l.isBool()) return CCValue(l.toBool() != r.toBool());
                if (l.isUndefined()) return CCValue(false);
                return CCValue(l.toNumber() != r.toNumber());
            }
            if (l.isBool()) l = CCValue(l.toNumber());
            if (r.isBool()) r = CCValue(r.toNumber());
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

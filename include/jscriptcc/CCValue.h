#pragma once
#include <string>
#include <cmath>
#include <climits>
#include <limits>

namespace jscriptcc {

/// Variant type for JScript CC values: number, string, or boolean.
/// Follows JScript's type coercion rules for conditional compilation.
class CCValue {
public:
    enum class Type { Number, String, Bool, Undefined };

    CCValue() : type_(Type::Undefined), num_(0), bool_(false) {}
    CCValue(double n) : type_(Type::Number), num_(n), bool_(n != 0) {}
    CCValue(const std::string& s) : type_(Type::String), num_(0), bool_(false), str_(s) {
        // Try to convert to number if it looks numeric
        if (!s.empty()) {
            char* end = nullptr;
            double d = std::strtod(s.c_str(), &end);
            if (end != s.c_str() && *end == '\0') {
                num_ = d;
            }
        }
    }
    CCValue(bool b) : type_(Type::Bool), num_(b ? 1 : 0), bool_(b) {}

    Type type() const { return type_; }
    bool isUndefined() const { return type_ == Type::Undefined; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isBool() const { return type_ == Type::Bool; }

    double toNumber() const {
        switch (type_) {
            case Type::Number: return num_;
            case Type::Bool: return bool_ ? 1.0 : 0.0;
            case Type::String: return num_; // pre-computed in constructor
            case Type::Undefined: return std::numeric_limits<double>::quiet_NaN();
        }
        return 0;
    }

    bool toBool() const {
        switch (type_) {
            case Type::Bool: return bool_;
            case Type::Number: return num_ != 0 && !std::isnan(num_);
            case Type::String: return !str_.empty();
            case Type::Undefined: return false;
        }
        return false;
    }

    std::string toString() const {
        switch (type_) {
            case Type::String: return str_;
            case Type::Bool: return bool_ ? "true" : "false";
            case Type::Number: {
                // Safely check if the number is an integer without UB.
                // static_cast<int> is undefined when the value is out of int range,
                // so we range-check first using doubles (which can represent INT_MIN/INT_MAX).
                if (!std::isnan(num_) && !std::isinf(num_) &&
                    num_ >= static_cast<double>(INT_MIN) &&
                    num_ <= static_cast<double>(INT_MAX)) {
                    int i = static_cast<int>(num_);
                    if (static_cast<double>(i) == num_) {
                        return std::to_string(i);
                    }
                }
                return std::to_string(num_);
            }
            case Type::Undefined: return "undefined";
        }
        return "";
    }

    const std::string& str() const { return str_; }

private:
    Type type_;
    double num_;
    bool bool_;
    std::string str_;
};

} // namespace jscriptcc

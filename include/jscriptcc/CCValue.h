#pragma once
#include <string>
#include <cmath>
#include <limits>

namespace jscriptcc {

/// Variant type for JScript CC values: number, string, or boolean.
/// Follows JScript's type coercion rules for conditional compilation.
class CCValue {
public:
    enum Type { TYPE_NUMBER, TYPE_STRING, TYPE_BOOL, TYPE_UNDEFINED };

    CCValue() : type_(TYPE_UNDEFINED), num_(0), bool_(false) {}
    CCValue(double n) : type_(TYPE_NUMBER), num_(n), bool_(n != 0) {}
    CCValue(const std::string& s) : type_(TYPE_STRING), num_(0), bool_(false), str_(s) {
        // Try to convert to number if it looks numeric
        if (!s.empty()) {
            char* end = nullptr;
            double d = std::strtod(s.c_str(), &end);
            if (end != s.c_str() && *end == '\0') {
                num_ = d;
            }
        }
    }
    CCValue(bool b) : type_(TYPE_BOOL), num_(b ? 1 : 0), bool_(b) {}

    Type type() const { return type_; }
    bool isUndefined() const { return type_ == TYPE_UNDEFINED; }
    bool isNumber() const { return type_ == TYPE_NUMBER; }
    bool isString() const { return type_ == TYPE_STRING; }
    bool isBool() const { return type_ == TYPE_BOOL; }

    double toNumber() const {
        switch (type_) {
            case TYPE_NUMBER: return num_;
            case TYPE_BOOL: return bool_ ? 1.0 : 0.0;
            case TYPE_STRING: return num_; // pre-computed in constructor
            case TYPE_UNDEFINED: return std::numeric_limits<double>::quiet_NaN();
        }
        return 0;
    }

    bool toBool() const {
        switch (type_) {
            case TYPE_BOOL: return bool_;
            case TYPE_NUMBER: return num_ != 0 && !std::isnan(num_);
            case TYPE_STRING: return !str_.empty();
            case TYPE_UNDEFINED: return false;
        }
        return false;
    }

    std::string toString() const {
        switch (type_) {
            case TYPE_STRING: return str_;
            case TYPE_BOOL: return bool_ ? "true" : "false";
            case TYPE_NUMBER: {
                if (num_ == static_cast<int>(num_) && !std::isnan(num_) && !std::isinf(num_)) {
                    return std::to_string(static_cast<int>(num_));
                }
                return std::to_string(num_);
            }
            case TYPE_UNDEFINED: return "undefined";
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

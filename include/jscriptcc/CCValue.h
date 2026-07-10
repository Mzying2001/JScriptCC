#pragma once
#include <string>

namespace jscriptcc {

/// Variant type for JScript CC values: number, string, or boolean.
/// Follows JScript's type coercion rules for conditional compilation.
class CCValue {
public:
    enum class Type { Number, String, Bool, Undefined };

    CCValue();
    CCValue(double number);
    CCValue(const std::string& string);
    CCValue(bool boolean);

    Type type() const { return type_; }
    bool isUndefined() const { return type_ == Type::Undefined; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isBool() const { return type_ == Type::Bool; }

    double toNumber() const;
    bool toBool() const;
    std::string toString() const;

    const std::string& str() const { return str_; }

private:
    Type type_;
    double num_;
    bool bool_;
    std::string str_;
};

} // namespace jscriptcc

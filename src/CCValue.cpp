#include "jscriptcc/CCValue.h"
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace jscriptcc {

CCValue::CCValue()
    : type_(Type::Undefined), num_(0), bool_(false) {}

CCValue::CCValue(double number)
    : type_(Type::Number), num_(number), bool_(number != 0) {}

CCValue::CCValue(const std::string& string)
    : type_(Type::String),
      num_(std::numeric_limits<double>::quiet_NaN()),
      bool_(false),
      str_(string) {
    const char* begin = string.c_str();
    while (*begin != '\0' && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    if (*begin == '\0') {
        num_ = 0.0;
        return;
    }

    char* end = nullptr;
    double number = std::strtod(begin, &end);
    while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }
    if (end != begin && end && *end == '\0') {
        num_ = number;
    }
}

CCValue::CCValue(bool boolean)
    : type_(Type::Bool), num_(boolean ? 1 : 0), bool_(boolean) {}

double CCValue::toNumber() const {
    switch (type_) {
        case Type::Number: return num_;
        case Type::Bool: return bool_ ? 1.0 : 0.0;
        case Type::String: return num_;
        case Type::Undefined: return std::numeric_limits<double>::quiet_NaN();
    }
    return 0;
}

bool CCValue::toBool() const {
    switch (type_) {
        case Type::Bool: return bool_;
        case Type::Number: return num_ != 0 && !std::isnan(num_);
        case Type::String: return !str_.empty();
        case Type::Undefined: return false;
    }
    return false;
}

std::string CCValue::toString() const {
    switch (type_) {
        case Type::String: return str_;
        case Type::Bool: return bool_ ? "true" : "false";
        case Type::Number: {
            if (!std::isnan(num_) && !std::isinf(num_) &&
                num_ >= static_cast<double>(INT_MIN) &&
                num_ <= static_cast<double>(INT_MAX)) {
                int integer = static_cast<int>(num_);
                if (static_cast<double>(integer) == num_) {
                    return std::to_string(integer);
                }
            }
            return std::to_string(num_);
        }
        case Type::Undefined: return "undefined";
    }
    return "";
}

} // namespace jscriptcc

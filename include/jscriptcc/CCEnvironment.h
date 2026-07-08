#pragma once
#include "CCValue.h"
#include <unordered_map>
#include <string>

namespace jscriptcc {

/// Holds predefined and user-set variables for conditional compilation.
class CCEnvironment {
public:
    CCEnvironment();
    void setDefaults();

    const CCValue* find(const std::string& name) const;
    void set(const std::string& name, const CCValue& value);

private:
    std::unordered_map<std::string, CCValue> variables;
};

} // namespace jscriptcc

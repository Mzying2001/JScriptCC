#pragma once
#include "CCValue.h"
#include <unordered_map>
#include <string>

namespace jscriptcc {

enum class TargetArchitecture {
    Win32,
    Win64
};

/// Holds predefined and user-set variables for conditional compilation.
class CCEnvironment {
public:
    explicit CCEnvironment(TargetArchitecture architecture = TargetArchitecture::Win32);
    void setDefaults();

    const CCValue* find(const std::string& name) const;
    void set(const std::string& name, const CCValue& value);

private:
    TargetArchitecture architecture_;
    std::unordered_map<std::string, CCValue> variables;
};

} // namespace jscriptcc

#include "jscriptcc/CCEnvironment.h"

namespace jscriptcc {

CCEnvironment::CCEnvironment(TargetArchitecture architecture)
    : architecture_(architecture) {
    initializeDefaults();
}

void CCEnvironment::initializeDefaults() {
    // JScript conditional compilation predefined variables.
    // These match Microsoft JScript's defaults for IE.
    variables_["@_jscript"] = CCValue(1.0);
    variables_["@_jscript_version"] = CCValue(5.8);  // IE8+ JScript version
    variables_["@_win16"] = CCValue(0.0);
    if (architecture_ == TargetArchitecture::Win64) {
        variables_["@_win32"] = CCValue(0.0);
        variables_["@_win64"] = CCValue(1.0);
        variables_["@_x86"] = CCValue(0.0);
        variables_["@_amd64"] = CCValue(1.0);
    } else {
        variables_["@_win32"] = CCValue(1.0);
        variables_["@_win64"] = CCValue(0.0);
        variables_["@_x86"] = CCValue(1.0);
        variables_["@_amd64"] = CCValue(0.0);
    }
    variables_["@_mac"] = CCValue(0.0);
    variables_["@_debug"] = CCValue(0.0);
}

const CCValue* CCEnvironment::find(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return &it->second;
    }
    return nullptr;
}

void CCEnvironment::set(const std::string& name, const CCValue& value) {
    variables_[name] = value;
}

} // namespace jscriptcc

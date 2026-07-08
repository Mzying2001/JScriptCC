#include "jscriptcc/CCEnvironment.h"

namespace jscriptcc {

CCEnvironment::CCEnvironment() {
    setDefaults();
}

void CCEnvironment::setDefaults() {
    // JScript conditional compilation predefined variables.
    // These match Microsoft JScript's defaults for IE.
    variables["@_jscript"] = CCValue(1.0);
    variables["@_jscript_version"] = CCValue(5.8);  // IE8+ JScript version
    variables["@_win16"] = CCValue(0.0);
    if (sizeof(void*) == 8) {
        variables["@_win32"] = CCValue(0.0);
        variables["@_win64"] = CCValue(1.0);
        variables["@_x86"] = CCValue(0.0);
        variables["@_amd64"] = CCValue(1.0);
    } else {
        variables["@_win32"] = CCValue(1.0);
        variables["@_win64"] = CCValue(0.0);
        variables["@_x86"] = CCValue(1.0);
        variables["@_amd64"] = CCValue(0.0);
    }
    variables["@_mac"] = CCValue(0.0);
    variables["@_debug"] = CCValue(0.0);
}

const CCValue* CCEnvironment::find(const std::string& name) const {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return &it->second;
    }
    return nullptr;
}

void CCEnvironment::set(const std::string& name, const CCValue& value) {
    variables[name] = value;
}

} // namespace jscriptcc

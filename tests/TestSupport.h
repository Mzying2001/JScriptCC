#pragma once
#include "TestFramework.h"
#include "jscriptcc/CCEnvironment.h"
#include "jscriptcc/CCPreprocessor.h"
#include <chrono>
#include <cstring>
#include <sstream>

inline std::string process(
    const std::string& source,
    const jscriptcc::CCEnvironment& env = jscriptcc::CCEnvironment())
{
    jscriptcc::CCPreprocessor preprocessor;
    std::string output;
    preprocessor.Process(source, output, env);
    return output;
}

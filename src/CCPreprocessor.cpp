#include "jscriptcc/CCPreprocessor.h"
#include "detail/Scanner.h"
#include "detail/Generator.h"

namespace jscriptcc {

bool CCPreprocessor::process(
    const std::string& source,
    std::string& output,
    const CCEnvironment& env,
    CCErrorList* errors)
{
    return process(source.data(), source.size(), output, env, errors);
}

bool CCPreprocessor::process(
    const char* data,
    std::size_t size,
    std::string& output,
    const CCEnvironment& env,
    CCErrorList* errors)
{
    CCErrorList localErrors;
    CCErrorList* errPtr = errors ? errors : &localErrors;

    if (data == nullptr) {
        output.clear();
        if (size == 0) return true;
        errPtr->push_back(CCError(1, 1, "Input data is null with non-zero size"));
        return false;
    }

    // Identify conditional compilation blocks.
    Scanner scanner;
    if (!scanner.scan(data, size, errPtr)) {
        output.assign(data, size);
        return false;
    }

    // Generate output from the scanned segments.
    CCEnvironment mutableEnv = env;
    Generator generator;
    if (!generator.generate(data, size, scanner.segments(), mutableEnv, output, errPtr)) {
        output.assign(data, size);
        return false;
    }

    return true;
}

} // namespace jscriptcc

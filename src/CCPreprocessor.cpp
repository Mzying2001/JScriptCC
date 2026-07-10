#include "jscriptcc/CCPreprocessor.h"
#include "detail/Scanner.h"
#include "detail/Generator.h"

namespace jscriptcc {

bool CCPreprocessor::Process(
    const std::string& source,
    std::string& output,
    const CCEnvironment& env,
    std::vector<CCError>* errors)
{
    return Process(source.data(), source.size(), output, env, errors);
}

bool CCPreprocessor::Process(
    const char* data,
    std::size_t size,
    std::string& output,
    const CCEnvironment& env,
    std::vector<CCError>* errors)
{
    CCErrorList localErrors;
    CCErrorList* errPtr = errors ? errors : &localErrors;

    if (data == nullptr) {
        output.clear();
        if (size == 0) return true;
        errPtr->push_back(CCError(1, 1, "Input data is null with non-zero size"));
        return false;
    }

    // Step 1: Scan source to identify CC blocks
    Scanner scanner;
    if (!scanner.scan(data, size, errPtr)) {
        output.assign(data, size);
        return false;
    }

    // Step 2: Generate output
    CCEnvironment mutableEnv = env; // copy so @set can modify it
    Generator generator;
    if (!generator.generate(data, size, scanner.segments(), mutableEnv, output, errPtr)) {
        output.assign(data, size);
        return false;
    }

    return true;
}

} // namespace jscriptcc

#include "jscriptcc/CCPreprocessor.h"
#include "detail/Scanner.h"
#include "detail/Generator.h"
#include "detail/UnicodeConversion.h"

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

bool CCPreprocessor::process(
    const std::wstring& source,
    std::wstring& output,
    const CCEnvironment& env,
    CCErrorList* errors)
{
    return process(source.data(), source.size(), output, env, errors);
}

bool CCPreprocessor::process(
    const wchar_t* data,
    std::size_t size,
    std::wstring& output,
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

    std::string utf8Source;
    detail::UnicodeError conversionError;
    if (!detail::wideToUtf8(data, size, utf8Source, &conversionError)) {
        std::wstring original(data, size);
        output.swap(original);
        errPtr->push_back(CCError(
            conversionError.line,
            conversionError.column,
            "Invalid Unicode sequence in wide input"));
        return false;
    }

    std::string utf8Output;
    CCErrorList processErrors;
    bool ok = process(utf8Source, utf8Output, env, &processErrors);
    detail::remapUtf8ErrorColumns(data, size, processErrors, 0);
    errPtr->insert(errPtr->end(), processErrors.begin(), processErrors.end());
    if (!ok) {
        std::wstring original(data, size);
        output.swap(original);
        return false;
    }

    std::wstring wideOutput;
    if (!detail::utf8ToWide(
            utf8Output.data(), utf8Output.size(), wideOutput, &conversionError)) {
        std::wstring original(data, size);
        output.swap(original);
        errPtr->push_back(CCError(
            conversionError.line,
            conversionError.column,
            "Internal output is not valid UTF-8"));
        return false;
    }

    output.swap(wideOutput);
    return true;
}

} // namespace jscriptcc

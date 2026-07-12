#pragma once

#include "jscriptcc/CCError.h"
#include <cstddef>
#include <string>

namespace jscriptcc {
namespace detail {

struct UnicodeError {
    int line;
    int column;

    UnicodeError() : line(1), column(1) {}
};

bool wideToUtf8(
    const wchar_t* data,
    std::size_t size,
    std::string& output,
    UnicodeError* error = nullptr);

bool utf8ToWide(
    const char* data,
    std::size_t size,
    std::wstring& output,
    UnicodeError* error = nullptr);

void remapUtf8ErrorColumns(
    const wchar_t* data,
    std::size_t size,
    CCErrorList& errors,
    std::size_t firstError);

} // namespace detail
} // namespace jscriptcc

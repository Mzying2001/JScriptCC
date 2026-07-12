#include "detail/UnicodeConversion.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace jscriptcc {
namespace detail {

static_assert(
    sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4,
    "JScriptCC wide-character API requires 16-bit or 32-bit wchar_t");

namespace {

bool isHighSurrogate(unsigned int value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool isLowSurrogate(unsigned int value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

bool isSurrogate(unsigned int value) {
    return value >= 0xd800 && value <= 0xdfff;
}

void appendUtf8(std::string& output, unsigned int codePoint) {
    if (codePoint <= 0x7f) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}

int utf8Length(unsigned int codePoint) {
    if (codePoint <= 0x7f) return 1;
    if (codePoint <= 0x7ff) return 2;
    if (codePoint <= 0xffff) return 3;
    return 4;
}

bool readWideCodePoint(
    const wchar_t* data,
    std::size_t size,
    std::size_t position,
    unsigned int& codePoint,
    std::size_t& codeUnits)
{
    unsigned int first = sizeof(wchar_t) == 2
        ? static_cast<unsigned int>(static_cast<std::uint16_t>(data[position]))
        : static_cast<unsigned int>(data[position]);
    switch (sizeof(wchar_t)) {
        case 2:
            if (isHighSurrogate(first)) {
                if (position + 1 >= size) return false;
                unsigned int second = static_cast<unsigned int>(
                    static_cast<std::uint16_t>(data[position + 1]));
                if (!isLowSurrogate(second)) return false;
                codePoint = 0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00);
                codeUnits = 2;
                return true;
            }
            if (isLowSurrogate(first)) return false;
            codePoint = first;
            codeUnits = 1;
            return true;
        case 4:
            if (first > 0x10ffff || isSurrogate(first)) return false;
            codePoint = first;
            codeUnits = 1;
            return true;
    }
    return false;
}

bool readUtf8CodePoint(
    const char* data,
    std::size_t size,
    std::size_t position,
    unsigned int& codePoint,
    std::size_t& byteCount)
{
    unsigned int first = static_cast<unsigned char>(data[position]);
    if (first <= 0x7f) {
        codePoint = first;
        byteCount = 1;
        return true;
    }

    unsigned int minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) {
        codePoint = first & 0x1f;
        byteCount = 2;
        minimum = 0x80;
    } else if (first >= 0xe0 && first <= 0xef) {
        codePoint = first & 0x0f;
        byteCount = 3;
        minimum = 0x800;
    } else if (first >= 0xf0 && first <= 0xf4) {
        codePoint = first & 0x07;
        byteCount = 4;
        minimum = 0x10000;
    } else {
        return false;
    }

    if (byteCount > size - position) return false;
    for (std::size_t i = 1; i < byteCount; ++i) {
        unsigned int next = static_cast<unsigned char>(data[position + i]);
        if ((next & 0xc0) != 0x80) return false;
        codePoint = (codePoint << 6) | (next & 0x3f);
    }

    return codePoint >= minimum && codePoint <= 0x10ffff && !isSurrogate(codePoint);
}

void appendWide(std::wstring& output, unsigned int codePoint) {
    if (sizeof(wchar_t) == 2 && codePoint > 0xffff) {
        unsigned int value = codePoint - 0x10000;
        output.push_back(static_cast<wchar_t>(0xd800 + (value >> 10)));
        output.push_back(static_cast<wchar_t>(0xdc00 + (value & 0x3ff)));
        return;
    }
    output.push_back(static_cast<wchar_t>(codePoint));
}

struct ErrorTarget {
    int line;
    int utf8Column;
    std::size_t errorIndex;
};

bool targetLess(const ErrorTarget& left, const ErrorTarget& right) {
    if (left.line != right.line) return left.line < right.line;
    return left.utf8Column < right.utf8Column;
}

} // namespace

bool wideToUtf8(
    const wchar_t* data,
    std::size_t size,
    std::string& output,
    UnicodeError* error)
{
    output.clear();
    output.reserve(size);

    int line = 1;
    int column = 1;
    std::size_t position = 0;
    while (position < size) {
        unsigned int codePoint = 0;
        std::size_t codeUnits = 0;
        if (!readWideCodePoint(data, size, position, codePoint, codeUnits)) {
            if (error) {
                error->line = line;
                error->column = column;
            }
            return false;
        }

        appendUtf8(output, codePoint);
        position += codeUnits;
        if (codePoint == '\n') {
            ++line;
            column = 1;
        } else {
            column += static_cast<int>(codeUnits);
        }
    }
    return true;
}

bool utf8ToWide(
    const char* data,
    std::size_t size,
    std::wstring& output,
    UnicodeError* error)
{
    output.clear();
    output.reserve(size);

    int line = 1;
    int column = 1;
    std::size_t position = 0;
    while (position < size) {
        unsigned int codePoint = 0;
        std::size_t byteCount = 0;
        if (!readUtf8CodePoint(data, size, position, codePoint, byteCount)) {
            if (error) {
                error->line = line;
                error->column = column;
            }
            return false;
        }

        appendWide(output, codePoint);
        position += byteCount;
        if (codePoint == '\n') {
            ++line;
            column = 1;
        } else {
            column += (sizeof(wchar_t) == 2 && codePoint > 0xffff) ? 2 : 1;
        }
    }
    return true;
}

void remapUtf8ErrorColumns(
    const wchar_t* data,
    std::size_t size,
    CCErrorList& errors,
    std::size_t firstError)
{
    if (firstError >= errors.size()) return;

    std::vector<ErrorTarget> targets;
    targets.reserve(errors.size() - firstError);
    for (std::size_t i = firstError; i < errors.size(); ++i) {
        if (errors[i].line > 0 && errors[i].column > 0) {
            targets.push_back({errors[i].line, errors[i].column, i});
        }
    }
    std::sort(targets.begin(), targets.end(), targetLess);

    std::size_t targetPosition = 0;
    std::size_t sourcePosition = 0;
    int line = 1;
    int wideColumn = 1;
    int utf8Column = 1;

    while (sourcePosition < size && targetPosition < targets.size()) {
        unsigned int codePoint = 0;
        std::size_t codeUnits = 0;
        if (!readWideCodePoint(data, size, sourcePosition, codePoint, codeUnits)) return;

        int nextUtf8Column = utf8Column + utf8Length(codePoint);
        while (targetPosition < targets.size() &&
               targets[targetPosition].line == line &&
               targets[targetPosition].utf8Column < nextUtf8Column) {
            errors[targets[targetPosition].errorIndex].column = wideColumn;
            ++targetPosition;
        }

        sourcePosition += codeUnits;
        if (codePoint == '\n') {
            ++line;
            wideColumn = 1;
            utf8Column = 1;
        } else {
            wideColumn += static_cast<int>(codeUnits);
            utf8Column = nextUtf8Column;
        }
    }

    while (targetPosition < targets.size()) {
        if (targets[targetPosition].line == line) {
            errors[targets[targetPosition].errorIndex].column = wideColumn;
        }
        ++targetPosition;
    }
}

} // namespace detail
} // namespace jscriptcc

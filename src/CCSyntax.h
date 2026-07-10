#pragma once
#include <cctype>
#include <cstddef>
#include <cstring>

namespace jscriptcc {

enum class CCDirective {
    None,
    CCOn,
    If,
    Elif,
    Else,
    End,
    Set
};

inline bool isJSIdentifierChar(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) ||
           value == '_' || value == '$';
}

inline bool isRegexEnablingKeyword(const char* data, std::size_t length) {
    switch (length) {
        case 2:
            return std::memcmp(data, "in", 2) == 0;
        case 3:
            return std::memcmp(data, "new", 3) == 0;
        case 4:
            return std::memcmp(data, "void", 4) == 0 ||
                   std::memcmp(data, "case", 4) == 0;
        case 5:
            return std::memcmp(data, "throw", 5) == 0 ||
                   std::memcmp(data, "yield", 5) == 0;
        case 6:
            return std::memcmp(data, "return", 6) == 0 ||
                   std::memcmp(data, "typeof", 6) == 0 ||
                   std::memcmp(data, "delete", 6) == 0;
        case 10:
            return std::memcmp(data, "instanceof", 10) == 0;
        default:
            return false;
    }
}

inline bool isRegexPrefixPunctuator(char value) {
    switch (value) {
        case '(': case '[': case '{': case ',': case ';': case ':':
        case '=': case '!': case '&': case '|': case '^': case '~':
        case '+': case '-': case '*': case '%': case '?': case '<': case '>':
            return true;
        default:
            return false;
    }
}

inline CCDirective matchCCDirective(
    const char* data,
    std::size_t size,
    std::size_t position)
{
    struct DirectiveMatch {
        const char* name;
        std::size_t length;
        CCDirective directive;
    };

    static const DirectiveMatch directives[] = {
        {"@cc_on", 6, CCDirective::CCOn},
        {"@if", 3, CCDirective::If},
        {"@elif", 5, CCDirective::Elif},
        {"@else", 5, CCDirective::Else},
        {"@end", 4, CCDirective::End},
        {"@set", 4, CCDirective::Set}
    };

    for (const auto& candidate : directives) {
        if (position + candidate.length <= size &&
            std::memcmp(data + position, candidate.name, candidate.length) == 0 &&
            (position + candidate.length == size ||
             !isJSIdentifierChar(data[position + candidate.length]))) {
            return candidate.directive;
        }
    }

    return CCDirective::None;
}

} // namespace jscriptcc

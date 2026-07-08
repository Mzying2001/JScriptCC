#pragma once
#include <string>
#include <vector>

namespace jscriptcc {

struct CCError {
    int line;
    int column;
    std::string message;

    CCError() : line(0), column(0) {}
    CCError(int l, int c, const std::string& msg) : line(l), column(c), message(msg) {}
};

using CCErrorList = std::vector<CCError>;

} // namespace jscriptcc

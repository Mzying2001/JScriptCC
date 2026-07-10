#include "support/TestFramework.h"
#include <iostream>

int main() {
    std::cout << "Running JScriptCC tests...\n\n";
    for (const auto& testCase : testRegistry()) {
        int failuresBefore = testsFailed();
        std::cout << "  [RUN ] " << testCase.name << "\n";
        testCase.func();
        std::cout << (testsFailed() == failuresBefore ? "  [PASS] " : "  [FAIL] ") << testCase.name << "\n";
    }
    std::cout << "\n========================================\n";
    std::cout << "Results: " << testsPassed() << " passed, " << testsFailed() << " failed, " << testsTotal() << " total assertions\n";
    std::cout << "========================================\n";
    return testsFailed() > 0 ? 1 : 0;
}

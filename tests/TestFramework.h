#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

std::vector<TestCase>& testRegistry();
int& testsPassed();
int& testsFailed();
int& testsTotal();

struct TestRegistrar {
    TestRegistrar(const char* name, const std::function<void()>& func);
};

#define TEST(name) \
    static void test_##name(); \
    static TestRegistrar reg_##name(#name, test_##name); \
    static void test_##name()

#define ASSERT_EQ(actual, expected) do { \
    testsTotal()++; \
    if ((actual) != (expected)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::cerr << "    Expected: [" << (expected) << "]\n"; \
        std::cerr << "    Actual:   [" << (actual) << "]\n"; \
        testsFailed()++; \
        return; \
    } \
    testsPassed()++; \
} while (0)

#define ASSERT_TRUE(expr) do { \
    testsTotal()++; \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::cerr << "    Expression was false: " << #expr << "\n"; \
        testsFailed()++; \
        return; \
    } \
    testsPassed()++; \
} while (0)

#define ASSERT_FALSE(expr) do { \
    testsTotal()++; \
    if ((expr)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::cerr << "    Expression was true: " << #expr << "\n"; \
        testsFailed()++; \
        return; \
    } \
    testsPassed()++; \
} while (0)

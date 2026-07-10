#include "TestFramework.h"

std::vector<TestCase>& testRegistry() { static std::vector<TestCase> registry; return registry; }
int& testsPassed() { static int value = 0; return value; }
int& testsFailed() { static int value = 0; return value; }
int& testsTotal() { static int value = 0; return value; }
TestRegistrar::TestRegistrar(const char* name, const std::function<void()>& func) { testRegistry().push_back({name, func}); }

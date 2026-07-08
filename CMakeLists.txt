cmake_minimum_required(VERSION 3.10)
project(JScriptCC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library sources
set(LIB_SOURCES
    src/StringSlice.cpp
    src/Scanner.cpp
    src/Tokenizer.cpp
    src/AST.cpp
    src/Parser.cpp
    src/Evaluator.cpp
    src/Generator.cpp
    src/CCPreprocessor.cpp
    src/CCEnvironment.cpp
)

add_library(jscriptcc STATIC ${LIB_SOURCES})
target_include_directories(jscriptcc PUBLIC include)

# Tests
enable_testing()
add_executable(jscriptcc_tests tests/main.cpp)
target_link_libraries(jscriptcc_tests jscriptcc)
add_test(NAME jscriptcc_tests COMMAND jscriptcc_tests)

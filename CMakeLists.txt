cmake_minimum_required(VERSION 3.10)

project(
    JScriptCC
    VERSION 1.0.0
    DESCRIPTION "JScript conditional compilation preprocessor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
target_include_directories(jscriptcc PRIVATE src)

# Tests
enable_testing()
add_executable(jscriptcc_tests tests/main.cpp)
target_link_libraries(jscriptcc_tests jscriptcc)
add_test(NAME jscriptcc_tests COMMAND jscriptcc_tests)

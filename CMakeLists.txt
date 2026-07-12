cmake_minimum_required(VERSION 3.10)

project(
    JScriptCC
    VERSION 1.2.0
    DESCRIPTION "JScript conditional compilation preprocessor"
    LANGUAGES CXX
)

# Default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Library sources
set(LIB_SOURCES
    src/CCValue.cpp
    src/Scanner.cpp
    src/Tokenizer.cpp
    src/Parser.cpp
    src/Evaluator.cpp
    src/Generator.cpp
    src/CCPreprocessor.cpp
    src/CCEnvironment.cpp
    src/UnicodeConversion.cpp
)

add_library(jscriptcc STATIC ${LIB_SOURCES})
target_include_directories(jscriptcc PUBLIC include)
target_include_directories(jscriptcc PRIVATE src)
target_compile_features(jscriptcc PUBLIC cxx_std_11)
set_target_properties(jscriptcc PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(jscriptcc PRIVATE /W4)
else()
    target_compile_options(jscriptcc PRIVATE -Wall -Wextra -pedantic)
endif()

# Tests
enable_testing()
add_executable(jscriptcc_tests
    tests/main.cpp
    tests/support/TestFramework.cpp
    tests/cases/test_preprocessor.cpp
    tests/cases/test_scanner.cpp
    tests/cases/test_expressions.cpp
    tests/cases/test_environment.cpp
    tests/cases/test_errors.cpp
    tests/cases/test_regressions.cpp
    tests/cases/test_wide_api.cpp
)
target_link_libraries(jscriptcc_tests jscriptcc)
target_include_directories(jscriptcc_tests PRIVATE tests)
set_target_properties(jscriptcc_tests PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(jscriptcc_tests PRIVATE /W4)
else()
    target_compile_options(jscriptcc_tests PRIVATE -Wall -Wextra -pedantic)
endif()

add_test(NAME jscriptcc_tests COMMAND jscriptcc_tests)

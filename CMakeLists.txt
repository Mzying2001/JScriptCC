cmake_minimum_required(VERSION 3.10)

project(
    JScriptCC
    VERSION 1.0.0
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
    tests/TestFramework.cpp
    tests/test_preprocessor.cpp
    tests/test_scanner.cpp
    tests/test_expressions.cpp
    tests/test_environment.cpp
    tests/test_errors.cpp
    tests/test_regressions.cpp
)
target_link_libraries(jscriptcc_tests jscriptcc)
set_target_properties(jscriptcc_tests PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(jscriptcc_tests PRIVATE /W4)
else()
    target_compile_options(jscriptcc_tests PRIVATE -Wall -Wextra -pedantic)
endif()

add_test(NAME jscriptcc_tests COMMAND jscriptcc_tests)

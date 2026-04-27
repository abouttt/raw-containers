cmake_minimum_required(VERSION 3.28)
project("raw-containers" LANGUAGES CXX)

cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CXX_COMPILER_ID:MSVC>:$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")

if(MSVC)
    add_compile_options(/Zc:preprocessor)
endif()

add_library(raw INTERFACE)
target_include_directories(raw INTERFACE include)
target_compile_features(raw INTERFACE cxx_std_20)

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(examples)

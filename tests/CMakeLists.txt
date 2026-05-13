include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(raw_tests ${TEST_SOURCES})

target_link_libraries(raw_tests PRIVATE raw gtest_main)
target_compile_features(raw_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(raw_tests PRIVATE /W4)
else()
    target_compile_options(raw_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(raw_tests)

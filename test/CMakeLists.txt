# doctest via FetchContent
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE
)
# doctest 2.4.11 ships a very old cmake_minimum_required that CMake 4.x rejects;
# allow its build to configure under the modern CMake policy set.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(doctest)
unset(CMAKE_POLICY_VERSION_MINIMUM)

set(EXPRESSIONIST_TEST_SOURCES
  test_main.cpp
  test_arithmetic.cpp
  test_functions.cpp
  test_booleans.cpp
  test_ordering.cpp
  test_errors.cpp
  test_nested.cpp
  test_produce.cpp
  test_overloads.cpp
  test_ranges.cpp
  test_benchmark.cpp
)

add_executable(expressionist_tests ${EXPRESSIONIST_TEST_SOURCES})
target_link_libraries(expressionist_tests PRIVATE
  expressionist
  doctest::doctest
)

if(MSVC)
  # doctest 2.4.11 declares a std::tuple specialization that the latest MSVC
  # flags as C5285 (forbidden by N5014) under C++20. It is benign and lives
  # entirely inside the dependency's header, so silence just that code here.
  target_compile_options(expressionist_tests PRIVATE /wd5285)
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(expressionist_tests)

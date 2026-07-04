cmake_minimum_required(VERSION 3.20)
project(Expressionist VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# --- Dependency: nlohmann/json -------------------------------------------
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# --- The header-only library ---------------------------------------------
add_library(expressionist INTERFACE)
add_library(Expressionist::Expressionist ALIAS expressionist)

target_include_directories(expressionist INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(expressionist INTERFACE cxx_std_20)
target_link_libraries(expressionist INTERFACE nlohmann_json::nlohmann_json)

# --- Tools, tests and examples (only when built as the top-level project) --
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(EXPRESSIONIST_BUILD_TESTS "Build the Expressionist test suite" ON)
  option(EXPRESSIONIST_BUILD_EXAMPLES "Build the Expressionist examples" ON)
  option(EXPRESSIONIST_BUILD_TOOL "Build the expressionist command-line tool" OFF)
  option(EXPRESSIONIST_BUILD_C_API "Build the expressionist_c shared library (plain C ABI)" OFF)
  option(EXPRESSIONIST_BUILD_PYTHON "Build and install the Python wrapper (implies EXPRESSIONIST_BUILD_C_API)" OFF)

  if(EXPRESSIONIST_BUILD_PYTHON)
    set(EXPRESSIONIST_BUILD_C_API ON)
  endif()

  if(EXPRESSIONIST_BUILD_EXAMPLES)
    add_executable(expressionist_example example/main.cpp)
    target_link_libraries(expressionist_example PRIVATE expressionist)
  endif()

  if(EXPRESSIONIST_BUILD_TOOL)
    # cxxopts for command-line parsing.
    set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      cxxopts
      GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
      GIT_TAG v3.2.0
      GIT_SHALLOW TRUE
    )
    # cxxopts 3.2.0 ships an old cmake_minimum_required that CMake 4.x rejects.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_MakeAvailable(cxxopts)
    unset(CMAKE_POLICY_VERSION_MINIMUM)

    add_executable(expressionist_cli cli/main.cpp)
    set_target_properties(expressionist_cli PROPERTIES OUTPUT_NAME expressionist)
    target_link_libraries(expressionist_cli PRIVATE expressionist cxxopts::cxxopts)
    target_compile_definitions(expressionist_cli PRIVATE
      EXPRESSIONIST_VERSION="${PROJECT_VERSION}")
  endif()

  if(EXPRESSIONIST_BUILD_C_API)
    include(GNUInstallDirs)

    add_library(expressionist_c SHARED src/expressionist_c.cpp)
    target_include_directories(expressionist_c PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(expressionist_c PRIVATE expressionist)
    target_compile_definitions(expressionist_c PRIVATE
      EXPRESSIONIST_C_BUILDING
      EXPRESSIONIST_VERSION="${PROJECT_VERSION}")
    set_target_properties(expressionist_c PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN ON
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR})

    install(TARGETS expressionist_c
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/expressionist_c.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  endif()

  if(EXPRESSIONIST_BUILD_PYTHON)
    # Interpreter is enough to resolve Python3_SITELIB; it honours an active
    # venv (Python3_FIND_VIRTUALENV defaults to preferring $VIRTUAL_ENV).
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    install(FILES python/expressionist/__init__.py
      DESTINATION ${Python3_SITELIB}/expressionist)
    install(TARGETS expressionist_c
      LIBRARY DESTINATION ${Python3_SITELIB}/expressionist
      RUNTIME DESTINATION ${Python3_SITELIB}/expressionist)
  endif()

  if(EXPRESSIONIST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
  endif()
endif()

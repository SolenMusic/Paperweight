cmake_minimum_required(VERSION 3.24)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND
   NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version supported by Paperweight")
endif()

project(Paperweight VERSION 0.0.29 LANGUAGES CXX)

option(PAPERWEIGHT_BUILD_TESTS "Build the Paperweight test suite" ON)
option(PAPERWEIGHT_BUILD_MACOS_APP "Build the native macOS application" ${APPLE})
option(PAPERWEIGHT_BUILD_EXAMPLES "Build Paperweight core examples" ON)
option(PAPERWEIGHT_BUILD_TOOLS "Build Paperweight command-line tools" ON)
option(PAPERWEIGHT_BUILD_BENCHMARKS "Build the Paperweight performance benchmarks" OFF)
set(PAPERWEIGHT_THREADS_DEFAULT ON)
if(EMSCRIPTEN)
    set(PAPERWEIGHT_THREADS_DEFAULT OFF)
endif()
option(PAPERWEIGHT_ENABLE_THREADS "Enable native multithreaded generation" ${PAPERWEIGHT_THREADS_DEFAULT})

function(paperweight_enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
        )
    endif()
endfunction()

add_library(paperweight_core STATIC
    src/core/evaluation.cpp
    src/core/generator.cpp
    src/core/graph.cpp
    src/core/graph_evaluator.cpp
    src/core/hash.cpp
    src/core/image.cpp
    src/core/material.cpp
    src/core/material_library.cpp
    src/core/material_template.cpp
    src/core/material_wizard.cpp
    src/core/noise.cpp
    src/core/organic.cpp
    src/core/pmat.cpp
    src/core/pwlib.cpp
    src/core/course.cpp
    src/core/region.cpp
    src/core/region_detail.cpp
    src/core/sculpt.cpp
    src/core/scatter.cpp
    src/core/shape.cpp
    src/core/structural.cpp
    src/core/stylised_lighting.cpp
    src/core/surface.cpp
    src/core/textile.cpp
    src/core/version.cpp
)
add_library(Paperweight::Core ALIAS paperweight_core)

target_compile_features(paperweight_core PUBLIC cxx_std_20)
target_include_directories(paperweight_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(paperweight_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
paperweight_enable_project_warnings(paperweight_core)

if(PAPERWEIGHT_ENABLE_THREADS)
    find_package(Threads REQUIRED)
    target_link_libraries(paperweight_core PRIVATE Threads::Threads)
    target_compile_definitions(paperweight_core PRIVATE PAPERWEIGHT_ENABLE_THREADS=1)
else()
    target_compile_definitions(paperweight_core PRIVATE PAPERWEIGHT_ENABLE_THREADS=0)
endif()

if(PAPERWEIGHT_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

if(PAPERWEIGHT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(PAPERWEIGHT_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(PAPERWEIGHT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(PAPERWEIGHT_BUILD_MACOS_APP)
    if(NOT APPLE)
        message(FATAL_ERROR "PAPERWEIGHT_BUILD_MACOS_APP requires macOS")
    endif()
    enable_language(OBJCXX)
    add_subdirectory(app/macos)
endif()

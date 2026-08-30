cmake_minimum_required(VERSION 3.24)

project(Paperweight VERSION 0.0.1 LANGUAGES CXX)

option(PAPERWEIGHT_BUILD_TESTS "Build the Paperweight test suite" ON)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version supported by Paperweight")
endif()

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

if(PAPERWEIGHT_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

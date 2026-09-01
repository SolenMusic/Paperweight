add_executable(paperweight_benchmarks
    benchmark_main.cpp
)

set(PAPERWEIGHT_BENCHMARK_MATERIAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/materials")
file(MAKE_DIRECTORY "${PAPERWEIGHT_BENCHMARK_MATERIAL_DIR}")

foreach(material IN ITEMS
    default
    brick-wall
    cobblestone
    ember
    cracked-stone
    weathered-metal
    mossy-pebbles
    knotty-wood
    marble-veins
    eroded-terrain
    toon-dungeon
    painted-metal
    graphic-marble
    region-stones
    castle-flagstone
    castle-stone
    cel-castle-stone
    castle-roof
    cel-forest-rock
    sculpted-flagstone
    worn-masonry
    sculpted-roof-slate
    castle-window
    detailed-crate
    decorative-fasteners
    masonry-corner-variation
    cel-courtyard-gravel
    scattered-debris
    foliage-foundation
)
    configure_file(
        "${PROJECT_SOURCE_DIR}/examples/materials/${material}.pmat"
        "${PAPERWEIGHT_BENCHMARK_MATERIAL_DIR}/${material}.pmat"
        COPYONLY
    )
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_config.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/benchmark_config.hpp"
    @ONLY
)

target_link_libraries(paperweight_benchmarks PRIVATE Paperweight::Core)
target_compile_features(paperweight_benchmarks PRIVATE cxx_std_20)
target_include_directories(paperweight_benchmarks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(paperweight_benchmarks PROPERTIES CXX_EXTENSIONS OFF)
paperweight_enable_project_warnings(paperweight_benchmarks)

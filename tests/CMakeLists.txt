add_executable(paperweight_tests
    test_main.cpp
)

configure_file(
    "${PROJECT_SOURCE_DIR}/examples/materials/default.pmat"
    "${CMAKE_CURRENT_BINARY_DIR}/default.pmat"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/materials/ember.pmat"
    "${CMAKE_CURRENT_BINARY_DIR}/ember.pmat"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/materials/brick-wall.pmat"
    "${CMAKE_CURRENT_BINARY_DIR}/brick-wall.pmat"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/materials/cobblestone.pmat"
    "${CMAKE_CURRENT_BINARY_DIR}/cobblestone.pmat"
    COPYONLY
)
foreach(showcase IN ITEMS
    cracked-stone
    weathered-metal
    mossy-pebbles
    knotty-wood
    marble-veins
    eroded-terrain
    toon-dungeon
    painted-metal
    graphic-marble
    polished-marble
    wet-mortar
    engraved-metal
    chrome
    steel
    copper
    brass
    painted-steel
    corroded-metal
    glazed-ceramic
    lacquered-wood
    wet-stone
    machinery-panels
    illuminated-scifi
    varnished-wood
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
    cel-forest-bark
    castle-foliage
    radial-target
    arc-and-crescent-motifs
)
    configure_file(
        "${PROJECT_SOURCE_DIR}/examples/materials/${showcase}.pmat"
        "${CMAKE_CURRENT_BINARY_DIR}/${showcase}.pmat"
        COPYONLY
    )
endforeach()

target_link_libraries(paperweight_tests PRIVATE Paperweight::Core)
target_compile_features(paperweight_tests PRIVATE cxx_std_20)
set_target_properties(paperweight_tests PROPERTIES CXX_EXTENSIONS OFF)
paperweight_enable_project_warnings(paperweight_tests)

add_test(NAME paperweight.core COMMAND paperweight_tests)

if(PAPERWEIGHT_BUILD_TOOLS)
    add_test(
        NAME paperweight.packer
        COMMAND $<TARGET_FILE:paperweight_pack>
            -o "${CMAKE_CURRENT_BINARY_DIR}/fixture-library.pwlib"
            --cpp-header "${CMAKE_CURRENT_BINARY_DIR}/fixture-library.h"
            --symbol fixture_library
            "${PROJECT_SOURCE_DIR}/tests/fixtures/library"
    )
endif()

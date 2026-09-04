find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

set(PAPERWEIGHT_METAL_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/Material3DPreviewShaders.metal"
)
set_source_files_properties(${PAPERWEIGHT_METAL_SHADER} PROPERTIES
    HEADER_FILE_ONLY TRUE
    MACOSX_PACKAGE_LOCATION "Resources/Shaders"
)

set(PAPERWEIGHT_SHOWCASE_MATERIALS
    "${PROJECT_SOURCE_DIR}/examples/materials/default.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/brick-wall.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/cobblestone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/ember.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/cracked-stone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/weathered-metal.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/mossy-pebbles.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/knotty-wood.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/marble-veins.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/eroded-terrain.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/toon-dungeon.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/painted-metal.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/graphic-marble.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/polished-marble.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/wet-mortar.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/engraved-metal.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/chrome.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/steel.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/copper.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/brass.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/painted-steel.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/corroded-metal.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/glazed-ceramic.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/lacquered-wood.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/wet-stone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/machinery-panels.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/illuminated-scifi.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/varnished-wood.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/region-stones.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/castle-flagstone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/castle-stone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/cel-castle-stone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/castle-roof.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/cel-forest-rock.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/sculpted-flagstone.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/worn-masonry.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/sculpted-roof-slate.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/castle-window.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/detailed-crate.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/decorative-fasteners.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/masonry-corner-variation.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/cel-courtyard-gravel.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/scattered-debris.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/foliage-foundation.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/cel-forest-bark.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/castle-foliage.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/radial-target.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/arc-and-crescent-motifs.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/woven-upholstery.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/alternating-carpet.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/heraldic-banner-cloth.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/attached-paving.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/arch-stone-panel.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/damaged-crate.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/detailed-target-panel.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/hierarchical-foliage.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/hierarchical-ground.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/rich-moss-colonies.pmat"
    "${PROJECT_SOURCE_DIR}/examples/materials/rich-lichen-stamps.pmat"
)
set_source_files_properties(${PAPERWEIGHT_SHOWCASE_MATERIALS} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/Showcases"
)

add_executable(Paperweight MACOSX_BUNDLE
    BenchmarkWindowController.hpp
    BenchmarkWindowController.mm
    ImageBridge.mm
    Material3DPreviewView.hpp
    Material3DPreviewView.mm
    MaterialLibraryWindowController.hpp
    MaterialLibraryWindowController.mm
    MaterialLibraryNavigatorController.hpp
    MaterialLibraryNavigatorController.mm
    PackedLibraryWindowController.hpp
    PackedLibraryWindowController.mm
    MaterialWizardWindowController.hpp
    MaterialWizardWindowController.mm
    main.mm
    ${PAPERWEIGHT_METAL_SHADER}
    ${PAPERWEIGHT_SHOWCASE_MATERIALS}
)

target_link_libraries(Paperweight PRIVATE
    Paperweight::Core
    ${APPKIT_FRAMEWORK}
    ${METAL_FRAMEWORK}
    ${METALKIT_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
)
target_compile_features(Paperweight PRIVATE cxx_std_20)
target_compile_options(Paperweight PRIVATE -fobjc-arc)
set_target_properties(Paperweight PROPERTIES
    CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "Paperweight"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.solenmusic.paperweight"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    OUTPUT_NAME "Paperweight"
)
paperweight_enable_project_warnings(Paperweight)

if(PAPERWEIGHT_BUILD_TESTS)
    add_executable(paperweight_macos_bridge_tests
        ImageBridge.mm
        bridge_tests.mm
    )
    target_link_libraries(paperweight_macos_bridge_tests PRIVATE
        Paperweight::Core
        ${APPKIT_FRAMEWORK}
    )
    target_compile_features(paperweight_macos_bridge_tests PRIVATE cxx_std_20)
    target_compile_options(paperweight_macos_bridge_tests PRIVATE -fobjc-arc)
    set_target_properties(paperweight_macos_bridge_tests PROPERTIES CXX_EXTENSIONS OFF)
    paperweight_enable_project_warnings(paperweight_macos_bridge_tests)
    add_test(NAME paperweight.macos_bridge COMMAND paperweight_macos_bridge_tests)
endif()

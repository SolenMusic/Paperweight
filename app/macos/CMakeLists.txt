find_library(APPKIT_FRAMEWORK AppKit REQUIRED)

add_executable(Paperweight MACOSX_BUNDLE
    main.mm
)

target_link_libraries(Paperweight PRIVATE
    Paperweight::Core
    ${APPKIT_FRAMEWORK}
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

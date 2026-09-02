add_executable(paperweight_pack
    pack_main.cpp
)

target_link_libraries(paperweight_pack PRIVATE Paperweight::Core)
target_compile_features(paperweight_pack PRIVATE cxx_std_20)
set_target_properties(paperweight_pack PROPERTIES CXX_EXTENSIONS OFF)
paperweight_enable_project_warnings(paperweight_pack)

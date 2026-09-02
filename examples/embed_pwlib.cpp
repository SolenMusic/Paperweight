// Generate paperweight_materials.h with paperweight_pack's --cpp-header and
// --symbol paperweight_materials options. See docs/pwlib-format.md.

#include "paperweight_materials.h"

#include <paperweight/generator.hpp>
#include <paperweight/pwlib.hpp>

#include <cstdint>
#include <iostream>
#include <span>
#include <variant>

int main()
{
    const auto bytes = std::span<const std::uint8_t>{
        reinterpret_cast<const std::uint8_t*>(paperweight_materials),
        paperweight_materials_size,
    };
    const auto opened = paperweight::readPwlib(bytes);
    const auto* library = std::get_if<paperweight::PackedMaterialLibrary>(&opened);
    if (library == nullptr || library->entries().empty()) {
        std::cerr << "Embedded Paperweight library is invalid or empty\n";
        return 1;
    }

    for (const auto& entry : library->entries()) {
        std::cout << entry.uid << "  " << entry.name << '\n';
    }
    const auto materialResult = library->instantiateByUid(
        library->entries().front().uid,
        18431);
    const auto* material = std::get_if<paperweight::Material>(&materialResult);
    if (material == nullptr) {
        std::cerr << "Material lookup failed\n";
        return 1;
    }
    paperweight::GenerationRequest request;
    request.material = *material;
    request.width = 256;
    request.height = 256;
    request.output = paperweight::MaterialOutput::colour;
    const auto generated = paperweight::generate(request);
    if (!std::holds_alternative<paperweight::Image>(generated)) {
        std::cerr << "Material generation failed\n";
        return 1;
    }
    return 0;
}

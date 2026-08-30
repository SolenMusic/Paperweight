#include <paperweight/generator.hpp>

#include <cstdint>
#include <iostream>
#include <variant>

int main()
{
    paperweight::GenerationRequest request;
    request.width = 256;
    request.height = 256;
    request.material.seed = 18431;

    auto result = paperweight::generate(request);
    if (const auto* error = std::get_if<paperweight::GenerationError>(&result)) {
        std::cerr << "Generation failed: " << error->message << '\n';
        return 1;
    }

    const auto& image = std::get<paperweight::Image>(result);
    std::uint64_t checksum = 1469598103934665603ULL;
    for (const auto& pixel : image.pixels()) {
        checksum ^= pixel.red;
        checksum *= 1099511628211ULL;
        checksum ^= pixel.green;
        checksum *= 1099511628211ULL;
        checksum ^= pixel.blue;
        checksum *= 1099511628211ULL;
        checksum ^= pixel.alpha;
        checksum *= 1099511628211ULL;
    }

    std::cout << image.width() << 'x' << image.height() << " RGBA8 checksum: " << checksum << '\n';
    return 0;
}

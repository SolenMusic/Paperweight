#include <paperweight/generator.hpp>
#include <paperweight/graph.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <variant>

namespace {

std::uint64_t checksum(std::span<const paperweight::Rgba8> pixels)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& pixel : pixels) {
        for (const auto channel : {pixel.red, pixel.green, pixel.blue, pixel.alpha}) {
            hash ^= channel;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

} // namespace

int main()
{
    paperweight::MaterialGraph graph;
    graph.nodes = {
        paperweight::GeneratorNode{
            1,
            std::nullopt,
            {},
            paperweight::NoiseOperation{17},
        },
        paperweight::GeneratorNode{
            2,
            std::nullopt,
            {},
            paperweight::CirclesOperation{8, 8, 0.31, 0.025},
        },
        paperweight::ProcessingNode{
            3,
            std::nullopt,
            paperweight::CompositeProcessing{
                1,
                2,
                std::nullopt,
                paperweight::CompositeMode::multiply,
                0.75,
            },
        },
        paperweight::OutputNode{4, paperweight::MaterialOutput::colour, 3},
        paperweight::OutputNode{5, paperweight::MaterialOutput::height, 1},
        paperweight::OutputNode{6, paperweight::MaterialOutput::normal, 2},
        paperweight::OutputNode{7, paperweight::MaterialOutput::roughness, 3},
    };

    if (const auto error = paperweight::validateMaterialGraph(graph)) {
        std::cerr << "Graph validation failed: " << error->message << '\n';
        return 1;
    }

    for (const auto output : paperweight::materialOutputs) {
        const auto result = paperweight::generate({
            paperweight::Material{},
            128,
            128,
            output,
            graph,
            std::nullopt,
        });
        const auto* image = std::get_if<paperweight::Image>(&result);
        if (image == nullptr) {
            std::cerr << "Generation failed: "
                      << std::get<paperweight::GenerationError>(result).message << '\n';
            return 1;
        }
        std::cout << paperweight::materialOutputName(output) << ": "
                  << checksum(image->pixels()) << '\n';
    }
    return 0;
}

#include "benchmark_config.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/graph.hpp>
#include <paperweight/image.hpp>
#include <paperweight/output.hpp>
#include <paperweight/pmat.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::array<std::string_view, 59> materialNames{
    "default",
    "brick-wall",
    "cobblestone",
    "ember",
    "cracked-stone",
    "weathered-metal",
    "mossy-pebbles",
    "knotty-wood",
    "marble-veins",
    "eroded-terrain",
    "toon-dungeon",
    "painted-metal",
    "graphic-marble",
    "region-stones",
    "castle-flagstone",
    "castle-stone",
    "cel-castle-stone",
    "castle-roof",
    "cel-forest-rock",
    "sculpted-flagstone",
    "worn-masonry",
    "sculpted-roof-slate",
    "castle-window",
    "detailed-crate",
    "decorative-fasteners",
    "masonry-corner-variation",
    "cel-courtyard-gravel",
    "scattered-debris",
    "foliage-foundation",
    "cel-forest-bark",
    "castle-foliage",
    "polished-marble",
    "wet-mortar",
    "engraved-metal",
    "varnished-wood",
    "chrome",
    "steel",
    "copper",
    "brass",
    "painted-steel",
    "corroded-metal",
    "glazed-ceramic",
    "lacquered-wood",
    "wet-stone",
    "machinery-panels",
    "illuminated-scifi",
    "woven-upholstery",
    "alternating-carpet",
    "heraldic-banner-cloth",
    "radial-target",
    "arc-and-crescent-motifs",
    "attached-paving",
    "arch-stone-panel",
    "damaged-crate",
    "detailed-target-panel",
    "hierarchical-foliage",
    "hierarchical-ground",
    "rich-moss-colonies",
    "rich-lichen-stamps",
};

constexpr auto materialOutputs = paperweight::materialOutputs;

struct Configuration {
    std::uint32_t resolution{256};
    std::uint32_t iterations{1};
    std::string material{"all"};
    std::string output{"all"};
    std::uint32_t workers{1};
};

std::uint32_t parsePositiveInteger(std::string_view text, std::string_view option)
{
    std::size_t consumed = 0;
    const auto value = std::stoul(std::string(text), &consumed);
    if (consumed != text.size() || value == 0 || value > 4096) {
        throw std::invalid_argument(std::string(option) + " must be between 1 and 4096");
    }
    return static_cast<std::uint32_t>(value);
}

void printUsage(const char* executable)
{
    std::cerr
        << "Usage: " << executable
        << " [--resolution N] [--iterations N] [--material NAME|all]"
           " [--output NAME|all|set] [--workers N|auto]\n";
}

Configuration parseArguments(int argc, char** argv)
{
    Configuration configuration;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument("missing value after " + std::string(argument));
        }
        const std::string_view value = argv[++index];
        if (argument == "--resolution") {
            configuration.resolution = parsePositiveInteger(value, argument);
        } else if (argument == "--iterations") {
            configuration.iterations = parsePositiveInteger(value, argument);
        } else if (argument == "--material") {
            configuration.material = value;
        } else if (argument == "--output") {
            configuration.output = value;
        } else if (argument == "--workers") {
            configuration.workers = value == "auto"
                ? 0U
                : parsePositiveInteger(value, argument);
        } else {
            throw std::invalid_argument("unknown option " + std::string(argument));
        }
    }
    return configuration;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("could not open " + path.string());
    }
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
}

paperweight::Material loadMaterial(std::string_view name)
{
    const auto path = std::filesystem::path(PAPERWEIGHT_BENCHMARK_MATERIAL_DIR) /
        (std::string(name) + ".pmat");
    auto parsed = paperweight::parsePmat(readFile(path));
    if (const auto* material = std::get_if<paperweight::Material>(&parsed)) {
        return *material;
    }
    const auto& diagnostic = std::get<paperweight::ParseDiagnostic>(parsed);
    throw std::runtime_error(
        path.string() + ":" + std::to_string(diagnostic.line) + ":" +
        std::to_string(diagnostic.column) + ": " + diagnostic.message);
}

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

std::string_view outputName(paperweight::MaterialOutput output)
{
    return paperweight::materialOutputName(output);
}

bool includesMaterial(const Configuration& configuration, std::string_view name)
{
    return configuration.material == "all" || configuration.material == name;
}

bool includesOutput(
    const Configuration& configuration,
    paperweight::MaterialOutput output)
{
    return configuration.output == "all" || configuration.output == outputName(output);
}

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values[middle - 1] + values[middle]) * 0.5;
    }
    return values[middle];
}

void benchmarkCompilation(std::string_view name, const paperweight::Material& material)
{
    constexpr std::uint32_t compileIterations = 101;
    std::vector<double> timings;
    timings.reserve(compileIterations);
    std::size_t nodeCount = 0;
    for (std::uint32_t iteration = 0; iteration < compileIterations; ++iteration) {
        const auto start = Clock::now();
        auto result = paperweight::compileMaterialGraph(material);
        const auto finish = Clock::now();
        const auto* graph = std::get_if<paperweight::MaterialGraph>(&result);
        if (graph == nullptr) {
            throw std::runtime_error("graph compilation failed for " + std::string(name));
        }
        nodeCount = graph->nodes.size();
        timings.push_back(std::chrono::duration<double, std::micro>(finish - start).count());
    }
    std::cout << "compile," << name << ",graph,0,0," << compileIterations << ",1,"
              << std::fixed << std::setprecision(3) << median(timings) << ','
              << *std::min_element(timings.begin(), timings.end()) << ",0," << nodeCount
              << '\n';
}

void benchmarkGeneration(
    const Configuration& configuration,
    std::string_view name,
    const paperweight::Material& material,
    const paperweight::MaterialGraph& graph,
    paperweight::MaterialOutput output)
{
    std::vector<double> timings;
    timings.reserve(configuration.iterations);
    std::uint64_t resultChecksum = 0;
    for (std::uint32_t iteration = 0; iteration < configuration.iterations; ++iteration) {
        const paperweight::GenerationRequest request{
            material,
            configuration.resolution,
            configuration.resolution,
            output,
            graph,
            std::nullopt,
            configuration.workers,
        };
        const auto start = Clock::now();
        auto result = paperweight::generate(request);
        const auto finish = Clock::now();
        const auto* image = std::get_if<paperweight::Image>(&result);
        if (image == nullptr) {
            throw std::runtime_error("generation failed for " + std::string(name));
        }
        const auto currentChecksum = checksum(image->pixels());
        if (iteration != 0 && currentChecksum != resultChecksum) {
            throw std::runtime_error("repeated generation changed bytes for " + std::string(name));
        }
        resultChecksum = currentChecksum;
        timings.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
    }

    const double medianMilliseconds = median(timings);
    const double megapixels =
        static_cast<double>(configuration.resolution) * configuration.resolution / 1'000'000.0;
    const double megapixelsPerSecond = megapixels / (medianMilliseconds / 1000.0);
    std::cout << "generate," << name << ',' << outputName(output) << ','
              << configuration.resolution << ',' << configuration.resolution << ','
              << configuration.iterations << ',' << configuration.workers << ','
              << std::fixed << std::setprecision(3)
              << medianMilliseconds << ','
              << *std::min_element(timings.begin(), timings.end()) << ','
              << megapixelsPerSecond << ',' << resultChecksum << '\n';
}

void benchmarkSequentialMaterialSet(
    const Configuration& configuration,
    std::string_view name,
    const paperweight::Material& material,
    const paperweight::MaterialGraph& graph)
{
    std::vector<double> timings;
    timings.reserve(configuration.iterations);
    std::uint64_t resultChecksum = 0;
    for (std::uint32_t iteration = 0; iteration < configuration.iterations; ++iteration) {
        const auto start = Clock::now();
        std::uint64_t combinedChecksum = 1469598103934665603ULL;
        for (const auto output : materialOutputs) {
            const paperweight::GenerationRequest request{
                material,
                configuration.resolution,
                configuration.resolution,
                output,
                graph,
                std::nullopt,
                configuration.workers,
            };
            auto result = paperweight::generate(request);
            const auto* image = std::get_if<paperweight::Image>(&result);
            if (image == nullptr) {
                throw std::runtime_error("material-set generation failed for " + std::string(name));
            }
            const auto imageChecksum = checksum(image->pixels());
            combinedChecksum ^= imageChecksum;
            combinedChecksum *= 1099511628211ULL;
        }
        const auto finish = Clock::now();
        if (iteration != 0 && combinedChecksum != resultChecksum) {
            throw std::runtime_error("repeated material-set generation changed bytes for " +
                                     std::string(name));
        }
        resultChecksum = combinedChecksum;
        timings.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
    }

    const double medianMilliseconds = median(timings);
    const double megapixels = static_cast<double>(configuration.resolution) *
        configuration.resolution * materialOutputs.size() / 1'000'000.0;
    const double megapixelsPerSecond = megapixels / (medianMilliseconds / 1000.0);
    std::cout << "generate_set_legacy," << name << ",set,"
              << configuration.resolution << ',' << configuration.resolution << ','
              << configuration.iterations << ',' << configuration.workers << ','
              << std::fixed << std::setprecision(3)
              << medianMilliseconds << ','
              << *std::min_element(timings.begin(), timings.end()) << ','
              << megapixelsPerSecond << ',' << resultChecksum << '\n';
}

void benchmarkMaterialSet(
    const Configuration& configuration,
    std::string_view name,
    const paperweight::Material& material,
    const paperweight::MaterialGraph& graph)
{
    std::vector<double> timings;
    timings.reserve(configuration.iterations);
    std::uint64_t resultChecksum = 0;
    for (std::uint32_t iteration = 0; iteration < configuration.iterations; ++iteration) {
        const paperweight::MaterialSetRequest request{
            material,
            configuration.resolution,
            configuration.resolution,
            paperweight::allMaterialOutputsSelected,
            graph,
            std::nullopt,
            configuration.workers,
        };
        const auto start = Clock::now();
        auto result = paperweight::generateMaterialSet(request);
        const auto finish = Clock::now();
        const auto* imageSet = std::get_if<paperweight::MaterialImageSet>(&result);
        if (imageSet == nullptr) {
            throw std::runtime_error("multi-output generation failed for " + std::string(name));
        }
        std::uint64_t combinedChecksum = 1469598103934665603ULL;
        for (const auto output : materialOutputs) {
            const auto* image = imageSet->image(output);
            if (image == nullptr) {
                throw std::runtime_error("multi-output result is incomplete for " +
                                         std::string(name));
            }
            combinedChecksum ^= checksum(image->pixels());
            combinedChecksum *= 1099511628211ULL;
        }
        if (iteration != 0 && combinedChecksum != resultChecksum) {
            throw std::runtime_error("repeated multi-output generation changed bytes for " +
                                     std::string(name));
        }
        resultChecksum = combinedChecksum;
        timings.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
    }

    const double medianMilliseconds = median(timings);
    const double megapixels = static_cast<double>(configuration.resolution) *
        configuration.resolution * materialOutputs.size() / 1'000'000.0;
    const double megapixelsPerSecond = megapixels / (medianMilliseconds / 1000.0);
    std::cout << "generate_set," << name << ",set,"
              << configuration.resolution << ',' << configuration.resolution << ','
              << configuration.iterations << ',' << configuration.workers << ','
              << std::fixed << std::setprecision(3)
              << medianMilliseconds << ','
              << *std::min_element(timings.begin(), timings.end()) << ','
              << megapixelsPerSecond << ',' << resultChecksum << '\n';
}

int run(const Configuration& configuration)
{
    const bool knownMaterial = configuration.material == "all" ||
        std::find(materialNames.begin(), materialNames.end(), configuration.material) !=
            materialNames.end();
    const bool knownOutput = configuration.output == "all" || configuration.output == "set" ||
        std::any_of(materialOutputs.begin(), materialOutputs.end(), [&](const auto output) {
            return outputName(output) == configuration.output;
        });
    if (!knownMaterial) {
        throw std::invalid_argument("unknown benchmark material " + configuration.material);
    }
    if (!knownOutput) {
        throw std::invalid_argument("unknown benchmark output " + configuration.output);
    }

    std::cout << "record,material,output,width,height,iterations,workers,median_time,min_time,"
                 "megapixels_per_second,checksum_or_nodes\n";
    for (const auto name : materialNames) {
        if (!includesMaterial(configuration, name)) {
            continue;
        }
        const auto material = loadMaterial(name);
        benchmarkCompilation(name, material);
        auto compilation = paperweight::compileMaterialGraph(material);
        const auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation);
        if (graph == nullptr) {
            throw std::runtime_error("graph compilation failed for " + std::string(name));
        }
        if (configuration.output == "set") {
            benchmarkSequentialMaterialSet(configuration, name, material, *graph);
            benchmarkMaterialSet(configuration, name, material, *graph);
        } else {
            for (const auto output : materialOutputs) {
                if (includesOutput(configuration, output)) {
                    benchmarkGeneration(configuration, name, material, *graph, output);
                }
            }
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return run(parseArguments(argc, argv));
    } catch (const std::exception& exception) {
        std::cerr << "paperweight_benchmarks: " << exception.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }
}

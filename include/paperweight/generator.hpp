#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <paperweight/graph.hpp>
#include <paperweight/image.hpp>
#include <paperweight/material.hpp>
#include <paperweight/output.hpp>
#include <paperweight/physical.hpp>

namespace paperweight {

struct GenerationRequest {
    Material material;
    std::uint32_t width{512};
    std::uint32_t height{512};
    MaterialOutput output{MaterialOutput::colour};
    std::optional<MaterialGraph> graph;
    std::optional<PhysicalSize> physicalCoverage;
    // Zero selects an automatic count, one forces the reference serial path,
    // and larger values request a bounded deterministic worker pool.
    std::uint32_t workerCount{0};
};

using MaterialOutputSelection = std::array<bool, materialOutputs.size()>;

inline constexpr MaterialOutputSelection allMaterialOutputsSelected{
    true, true, true, true, true, true, true, true, true, true,
};

struct MaterialSetRequest {
    Material material;
    std::uint32_t width{512};
    std::uint32_t height{512};
    MaterialOutputSelection outputs{allMaterialOutputsSelected};
    std::optional<MaterialGraph> graph;
    std::optional<PhysicalSize> physicalCoverage;
    // Zero selects an automatic count, one forces the reference serial path,
    // and larger values request a bounded deterministic worker pool.
    std::uint32_t workerCount{0};
};

enum class GenerationErrorCode {
    invalidDimensions,
    invalidMaterial,
    invalidGraph,
    invalidOutput,
    invalidPhysicalCoverage,
    allocationFailure,
    cancelled,
};

struct GenerationError {
    GenerationErrorCode code;
    std::string message;
};

using GenerationResult = std::variant<Image, GenerationError>;
struct MaterialImageSet {
    std::array<std::optional<Image>, materialOutputs.size()> images;

    [[nodiscard]] Image* image(MaterialOutput output) noexcept
    {
        const auto index = materialOutputIndex(output);
        if (index >= images.size()) {
            return nullptr;
        }
        auto& value = images[index];
        return value ? &*value : nullptr;
    }

    [[nodiscard]] const Image* image(MaterialOutput output) const noexcept
    {
        const auto index = materialOutputIndex(output);
        if (index >= images.size()) {
            return nullptr;
        }
        const auto& value = images[index];
        return value ? &*value : nullptr;
    }
};
using MaterialSetResult = std::variant<MaterialImageSet, GenerationError>;
using GenerationCancellationCheck = std::function<bool()>;

[[nodiscard]] GenerationResult generate(const GenerationRequest& request);
[[nodiscard]] GenerationResult generate(
    const GenerationRequest& request,
    const GenerationCancellationCheck& isCancelled);
[[nodiscard]] MaterialSetResult generateMaterialSet(const MaterialSetRequest& request);
[[nodiscard]] MaterialSetResult generateMaterialSet(
    const MaterialSetRequest& request,
    const GenerationCancellationCheck& isCancelled);

// Returns the conservative set of generated maps that can change when one
// valid material value replaces another. Preview-only optical properties and
// catalogue metadata deliberately do not invalidate CPU images.
[[nodiscard]] MaterialOutputSelection affectedMaterialOutputs(
    const Material& before,
    const Material& after) noexcept;

} // namespace paperweight

#pragma once

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
using GenerationCancellationCheck = std::function<bool()>;

[[nodiscard]] GenerationResult generate(const GenerationRequest& request);
[[nodiscard]] GenerationResult generate(
    const GenerationRequest& request,
    const GenerationCancellationCheck& isCancelled);

} // namespace paperweight

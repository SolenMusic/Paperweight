#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

#include <paperweight/image.hpp>
#include <paperweight/material.hpp>

namespace paperweight {

enum class MaterialOutput {
    colour,
    height,
    normal,
    roughness,
};

struct GenerationRequest {
    Material material;
    std::uint32_t width{512};
    std::uint32_t height{512};
    MaterialOutput output{MaterialOutput::colour};
};

enum class GenerationErrorCode {
    invalidDimensions,
    invalidMaterial,
    invalidOutput,
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

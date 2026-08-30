#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include <paperweight/image.hpp>
#include <paperweight/material.hpp>

namespace paperweight {

struct GenerationRequest {
    Material material;
    std::uint32_t width{512};
    std::uint32_t height{512};
};

enum class GenerationErrorCode {
    invalidDimensions,
    invalidMaterial,
    allocationFailure,
};

struct GenerationError {
    GenerationErrorCode code;
    std::string message;
};

using GenerationResult = std::variant<Image, GenerationError>;

[[nodiscard]] GenerationResult generate(const GenerationRequest& request);

} // namespace paperweight

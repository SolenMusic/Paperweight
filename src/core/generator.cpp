#include <paperweight/generator.hpp>

#include <paperweight/noise.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace paperweight {
namespace {

constexpr std::uint32_t maximumDimension = 4096;

std::uint8_t toByte(double value)
{
    const double scaled = std::round(std::clamp(value, 0.0, 1.0) * 255.0);
    return static_cast<std::uint8_t>(scaled);
}

} // namespace

GenerationResult generate(const GenerationRequest& request)
{
    if (request.width == 0 || request.height == 0 || request.width > maximumDimension ||
        request.height > maximumDimension) {
        return GenerationError{
            GenerationErrorCode::invalidDimensions,
            "output dimensions must be between 1 and 4096 pixels",
        };
    }
    if (const auto error = validateMaterial(request.material)) {
        return GenerationError{GenerationErrorCode::invalidMaterial, *error};
    }

    try {
        Image image(request.width, request.height);
        for (std::uint32_t y = 0; y < request.height; ++y) {
            auto row = image.row(y);
            const double v = (static_cast<double>(y) + 0.5) / request.height;
            for (std::uint32_t x = 0; x < request.width; ++x) {
                const double u = (static_cast<double>(x) + 0.5) / request.width;
                const auto grey = toByte(periodicFbm2D(u, v, request.material));
                row[x] = Rgba8{grey, grey, grey, 255};
            }
        }
        return image;
    } catch (const std::exception& exception) {
        return GenerationError{GenerationErrorCode::allocationFailure, exception.what()};
    }
}

} // namespace paperweight
